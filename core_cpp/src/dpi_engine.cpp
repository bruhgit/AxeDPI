#include "dpi_engine.h"
#include "sni_parser.h"
#include "doh_resolver.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

namespace AxeDpi {

static bool s_wsaInitialized = false;

static void EnsureWsa() {
    if (!s_wsaInitialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            s_wsaInitialized = true;
        }
    }
}

DpiEngine::DpiEngine() {
    EnsureWsa();
}

DpiEngine::~DpiEngine() {
    Stop();
}

DpiEngine& DpiEngine::Instance() {
    static DpiEngine s_instance;
    return s_instance;
}

void DpiEngine::SetLogCallback(std::function<void(const std::string&)> cb) {
    m_logCallback = cb;
}

void DpiEngine::Log(const std::string& msg) {
    if (m_logCallback) {
        m_logCallback(msg);
    }
}

bool DpiEngine::IsRunning() const {
    return m_running.load();
}

void DpiEngine::GetStats(uint64_t& outBytesIn, uint64_t& outBytesOut, int& outActiveConns) const {
    outBytesIn = m_bytesIn.load();
    outBytesOut = m_bytesOut.load();
    outActiveConns = m_activeConns.load();
}

static void KillConflictingProcess(const std::wstring& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    DWORD myPid = GetCurrentProcessId();

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                if (pe.th32ProcessID != myPid) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

bool DpiEngine::Start(const DpiConfig& config) {
    if (m_running.load()) return true;

    m_config = config;
    EnsureWsa();

    // Silently terminate any conflicting previous Go axedpi.exe running in background
    KillConflictingProcess(L"axedpi.exe");

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        Log("[Error] Failed to create listen socket: " + std::to_string(WSAGetLastError()));
        return false;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(m_config.bindPort));
    inet_pton(AF_INET, m_config.bindHost.c_str(), &serverAddr.sin_addr);

    if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == 10013 || err == 10048) {
            // Port is held; force terminate stale axedpi.exe, wait for socket release and retry
            KillConflictingProcess(L"axedpi.exe");
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            if (bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
                goto BindOk;
            }
        }
        Log("[Error] Failed to bind to " + m_config.bindHost + ":" + std::to_string(m_config.bindPort) + " (Error: " + std::to_string(WSAGetLastError()) + "). Port may be used by another app; try changing port in Engine Parameters.");
        closesocket(listenSock);
        return false;
    }

BindOk:

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        Log("[Error] Failed to listen on socket");
        closesocket(listenSock);
        return false;
    }

    m_listenSock = static_cast<uintptr_t>(listenSock);
    m_running.store(true);

    Log("[Proxy] DPI Service started on " + m_config.bindHost + ":" + std::to_string(m_config.bindPort));

    m_serverThread = std::thread(&DpiEngine::ServerLoop, this);
    return true;
}

void DpiEngine::Stop() {
    if (!m_running.exchange(false)) return;

    if (m_listenSock != 0 && m_listenSock != (uintptr_t)INVALID_SOCKET) {
        closesocket(static_cast<SOCKET>(m_listenSock));
        m_listenSock = 0;
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    Log("[Proxy] DPI Service stopped.");
}

void DpiEngine::ServerLoop() {
    SOCKET listenSock = static_cast<SOCKET>(m_listenSock);

    while (m_running.load()) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);

        if (clientSock == INVALID_SOCKET) {
            if (!m_running.load()) break;
            continue;
        }

        // Set TCP_NODELAY
        int flag = 1;
        setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

        m_activeConns++;
        std::thread([this, clientSock]() {
            HandleClient(static_cast<uintptr_t>(clientSock));
            m_activeConns--;
        }).detach();
    }
}

static bool RecvExact(SOCKET s, uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int r = recv(s, reinterpret_cast<char*>(buf + total), static_cast<int>(len - total), 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

static std::string ReadLine(SOCKET s) {
    std::string line;
    char c = 0;
    while (recv(s, &c, 1, 0) > 0) {
        line += c;
        if (line.size() >= 2 && line[line.size() - 2] == '\r' && line[line.size() - 1] == '\n') {
            break;
        }
    }
    return line;
}

uintptr_t DpiEngine::DialTarget(const std::string& host, int port) {
    std::string targetIp = host;

    // Check if host is not an IPv4 address -> resolve using DoH
    in_addr addrCheck;
    if (inet_pton(AF_INET, host.c_str(), &addrCheck) != 1) {
        DohResponse doh = DohResolver::Instance().Resolve(host, m_config.verbose);
        if (doh.success && !doh.ips.empty()) {
            targetIp = doh.ips[0];
            if (m_config.verbose) {
                Log("[DNS] DoH resolved " + host + " -> " + targetIp + " (" + std::to_string(doh.latencyMs) + "ms)");
            }
        } else {
            // Fallback to system getaddrinfo
            addrinfo hints{}, *res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) == 0 && res) {
                sockaddr_in* ipv4 = (sockaddr_in*)res->ai_addr;
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
                targetIp = ipStr;
                freeaddrinfo(res);
            }
        }
    }

    SOCKET remoteSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (remoteSock == INVALID_SOCKET) return 0;

    int flag = 1;
    setsockopt(remoteSock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    // Connect timeout
    DWORD timeout = 5000;
    setsockopt(remoteSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(remoteSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in remoteAddr{};
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, targetIp.c_str(), &remoteAddr.sin_addr);

    if (connect(remoteSock, (sockaddr*)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR) {
        closesocket(remoteSock);
        return 0;
    }

    return static_cast<uintptr_t>(remoteSock);
}

void DpiEngine::HandleClient(uintptr_t clientSockRaw) {
    SOCKET clientSock = static_cast<SOCKET>(clientSockRaw);

    // Peek the first byte to detect protocol
    uint8_t firstByte = 0;
    int peeked = recv(clientSock, reinterpret_cast<char*>(&firstByte), 1, MSG_PEEK);
    if (peeked <= 0) {
        closesocket(clientSock);
        return;
    }

    if (firstByte == 0x05) {
        // SOCKS5 Protocol
        HandleSocks5(clientSockRaw, &firstByte);
    } else {
        // HTTP / HTTPS CONNECT
        std::string reqLine = ReadLine(clientSock);
        if (!reqLine.empty()) {
            HandleHttp(clientSockRaw, reqLine);
        } else {
            closesocket(clientSock);
        }
    }
}

void DpiEngine::HandleSocks5(uintptr_t clientSockRaw, const uint8_t* /*initialByte*/) {
    SOCKET clientSock = static_cast<SOCKET>(clientSockRaw);

    // 1. Auth negotiation
    uint8_t ver = 0;
    if (recv(clientSock, reinterpret_cast<char*>(&ver), 1, 0) <= 0 || ver != 0x05) {
        closesocket(clientSock);
        return;
    }

    uint8_t nMethods = 0;
    if (recv(clientSock, reinterpret_cast<char*>(&nMethods), 1, 0) <= 0) {
        closesocket(clientSock);
        return;
    }

    std::vector<uint8_t> methods(nMethods);
    if (!RecvExact(clientSock, methods.data(), nMethods)) {
        closesocket(clientSock);
        return;
    }

    // Reply: 0x05 (version), 0x00 (no auth required)
    uint8_t authReply[2] = {0x05, 0x00};
    send(clientSock, reinterpret_cast<const char*>(authReply), 2, 0);

    // 2. Request command
    uint8_t reqHeader[4] = {0};
    if (!RecvExact(clientSock, reqHeader, 4)) {
        closesocket(clientSock);
        return;
    }

    if (reqHeader[0] != 0x05 || reqHeader[1] != 0x01) { // Only CONNECT supported
        uint8_t failReply[10] = {0x05, 0x07, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        send(clientSock, reinterpret_cast<const char*>(failReply), 10, 0);
        closesocket(clientSock);
        return;
    }

    std::string targetHost;
    if (reqHeader[3] == 0x01) { // IPv4
        uint8_t ipBuf[4];
        if (!RecvExact(clientSock, ipBuf, 4)) { closesocket(clientSock); return; }
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, ipBuf, ipStr, INET_ADDRSTRLEN);
        targetHost = ipStr;
    } else if (reqHeader[3] == 0x03) { // Domain name
        uint8_t dLen = 0;
        if (!RecvExact(clientSock, &dLen, 1)) { closesocket(clientSock); return; }
        std::vector<char> dBuf(dLen + 1, 0);
        if (!RecvExact(clientSock, reinterpret_cast<uint8_t*>(dBuf.data()), dLen)) { closesocket(clientSock); return; }
        targetHost = dBuf.data();
    } else { // IPv6 not handled
        uint8_t failReply[10] = {0x05, 0x08, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        send(clientSock, reinterpret_cast<const char*>(failReply), 10, 0);
        closesocket(clientSock);
        return;
    }

    uint8_t portBuf[2];
    if (!RecvExact(clientSock, portBuf, 2)) {
        closesocket(clientSock);
        return;
    }
    int targetPort = (static_cast<int>(portBuf[0]) << 8) | portBuf[1];

    if (m_config.verbose) {
        Log("[SOCKS5] Target: " + targetHost + ":" + std::to_string(targetPort));
    }

    // Connect to target
    uintptr_t remoteSockRaw = DialTarget(targetHost, targetPort);
    if (remoteSockRaw == 0) {
        uint8_t failReply[10] = {0x05, 0x04, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        send(clientSock, reinterpret_cast<const char*>(failReply), 10, 0);
        closesocket(clientSock);
        return;
    }

    // Success response
    uint8_t okReply[10] = {0x05, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
    send(clientSock, reinterpret_cast<const char*>(okReply), 10, 0);

    TunnelWithDpi(clientSockRaw, remoteSockRaw);
}

void DpiEngine::HandleHttp(uintptr_t clientSockRaw, const std::string& reqLine) {
    SOCKET clientSock = static_cast<SOCKET>(clientSockRaw);

    std::istringstream iss(reqLine);
    std::string method, uri, httpVer;
    iss >> method >> uri >> httpVer;

    if (method == "CONNECT") {
        // Read remaining headers until \r\n\r\n
        while (true) {
            std::string header = ReadLine(clientSock);
            if (header.empty() || header == "\r\n" || header == "\n") break;
        }

        std::string host = uri;
        int port = 443;
        size_t colon = uri.find(':');
        if (colon != std::string::npos) {
            host = uri.substr(0, colon);
            port = std::stoi(uri.substr(colon + 1));
        }

        if (m_config.verbose) {
            Log("[HTTP CONNECT] Target: " + host + ":" + std::to_string(port));
        }

        uintptr_t remoteSockRaw = DialTarget(host, port);
        if (remoteSockRaw == 0) {
            const char* badGateway = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
            send(clientSock, badGateway, static_cast<int>(strlen(badGateway)), 0);
            closesocket(clientSock);
            return;
        }

        const char* okResp = "HTTP/1.1 200 Connection Established\r\n\r\n";
        send(clientSock, okResp, static_cast<int>(strlen(okResp)), 0);

        TunnelWithDpi(clientSockRaw, remoteSockRaw);
    } else {
        // Plain HTTP
        std::string fullHeaders = reqLine;
        while (true) {
            std::string header = ReadLine(clientSock);
            if (header.empty()) break;
            fullHeaders += header;
            if (header == "\r\n" || header == "\n") break;
        }

        std::string host = "127.0.0.1";
        int port = 80;
        size_t hostPos = fullHeaders.find("Host: ");
        if (hostPos == std::string::npos) hostPos = fullHeaders.find("host: ");
        if (hostPos != std::string::npos) {
            size_t endLine = fullHeaders.find("\r\n", hostPos);
            std::string hostVal = fullHeaders.substr(hostPos + 6, endLine - (hostPos + 6));
            size_t colon = hostVal.find(':');
            if (colon != std::string::npos) {
                host = hostVal.substr(0, colon);
                port = std::stoi(hostVal.substr(colon + 1));
            } else {
                host = hostVal;
            }
        }

        uintptr_t remoteSockRaw = DialTarget(host, port);
        if (remoteSockRaw == 0) {
            const char* badGateway = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
            send(clientSock, badGateway, static_cast<int>(strlen(badGateway)), 0);
            closesocket(clientSock);
            return;
        }

        // Apply DPI Header Modification: Host -> hOsT
        auto modBuf = ModifyHttpHeaders(reinterpret_cast<const uint8_t*>(fullHeaders.data()), fullHeaders.size());
        send(static_cast<SOCKET>(remoteSockRaw), reinterpret_cast<const char*>(modBuf.data()), static_cast<int>(modBuf.size()), 0);

        TunnelWithDpi(clientSockRaw, remoteSockRaw);
    }
}

void DpiEngine::TunnelWithDpi(uintptr_t clientSockRaw, uintptr_t remoteSockRaw) {
    SOCKET clientSock = static_cast<SOCKET>(clientSockRaw);
    SOCKET remoteSock = static_cast<SOCKET>(remoteSockRaw);

    std::atomic<bool> closed{false};

    // Forward Client -> Remote with DPI Bypass
    std::thread tUpstream([this, clientSock, remoteSock, &closed]() {
        bool firstPacket = true;
        std::vector<uint8_t> buffer(65536);

        while (!closed.load()) {
            if (firstPacket) {
                // Peek first 5 bytes for TLS record header
                uint8_t header[5] = {0};
                int nHeader = recv(clientSock, reinterpret_cast<char*>(header), 5, MSG_PEEK);
                if (nHeader <= 0) break;

                if (nHeader == 5 && header[0] == 0x16 && header[1] == 0x03) {
                    // TLS Handshake Record
                    uint16_t payloadLen = (static_cast<uint16_t>(header[3]) << 8) | header[4];
                    std::vector<uint8_t> fullRecord(5 + payloadLen);

                    // Drain the complete record from client
                    if (!RecvExact(clientSock, fullRecord.data(), fullRecord.size())) {
                        break;
                    }

                    uint8_t* handshakePayload = fullRecord.data() + 5;
                    if (payloadLen > 0 && handshakePayload[0] == 0x01) {
                        // Client Hello: analyze SNI
                        SniResult sni = FindSniOffset(handshakePayload, payloadLen);
                        int splitPos = m_config.splitPos;

                        if (sni.found && sni.length >= 2) {
                            // Dynamic SNI splitting: split domain right inside its characters
                            splitPos = sni.offset + 2;
                            if (m_config.verbose) {
                                Log("[DPI] Detected SNI: " + sni.hostname + " (Splitting domain at offset " + std::to_string(splitPos) + ")");
                            }
                        } else if (splitPos <= 0 || splitPos >= static_cast<int>(payloadLen)) {
                            splitPos = 40;
                        }

                        if (splitPos > 0 && splitPos < static_cast<int>(payloadLen)) {
                            // Construct Fragment 1
                            size_t part1Len = static_cast<size_t>(splitPos);
                            std::vector<uint8_t> rec1(5 + part1Len);
                            std::memcpy(rec1.data(), fullRecord.data(), 3);
                            rec1[3] = static_cast<uint8_t>((part1Len >> 8) & 0xFF);
                            rec1[4] = static_cast<uint8_t>(part1Len & 0xFF);
                            std::memcpy(rec1.data() + 5, handshakePayload, part1Len);

                            // Construct Fragment 2
                            size_t part2Len = payloadLen - part1Len;
                            std::vector<uint8_t> rec2(5 + part2Len);
                            std::memcpy(rec2.data(), fullRecord.data(), 3);
                            rec2[3] = static_cast<uint8_t>((part2Len >> 8) & 0xFF);
                            rec2[4] = static_cast<uint8_t>(part2Len & 0xFF);
                            std::memcpy(rec2.data() + 5, handshakePayload + part1Len, part2Len);

                            // Send Fragment 1
                            send(remoteSock, reinterpret_cast<const char*>(rec1.data()), static_cast<int>(rec1.size()), 0);
                            m_bytesOut += rec1.size();

                            // Inter-fragment delay to force distinct TCP segment delivery
                            if (m_config.splitDelayMs > 0) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(m_config.splitDelayMs));
                            }

                            // Send Fragment 2
                            send(remoteSock, reinterpret_cast<const char*>(rec2.data()), static_cast<int>(rec2.size()), 0);
                            m_bytesOut += rec2.size();

                            if (m_config.verbose) {
                                Log("[DPI] TLS Fragmented into " + std::to_string(rec1.size()) + " and " + std::to_string(rec2.size()) + " bytes");
                            }

                            firstPacket = false;
                            continue;
                        }
                    }

                    // If not fragmented, send original full record
                    send(remoteSock, reinterpret_cast<const char*>(fullRecord.data()), static_cast<int>(fullRecord.size()), 0);
                    m_bytesOut += fullRecord.size();
                    firstPacket = false;
                    continue;
                }

                firstPacket = false;
            }

            int n = recv(clientSock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
            if (n <= 0) break;

            int sent = send(remoteSock, reinterpret_cast<const char*>(buffer.data()), n, 0);
            if (sent <= 0) break;
            m_bytesOut += sent;
        }

        closed.store(true);
        closesocket(clientSock);
        closesocket(remoteSock);
    });

    // Forward Remote -> Client (Direct high-throughput copy)
    std::thread tDownstream([this, clientSock, remoteSock, &closed]() {
        std::vector<uint8_t> buffer(65536);
        while (!closed.load()) {
            int n = recv(remoteSock, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
            if (n <= 0) break;

            int sent = send(clientSock, reinterpret_cast<const char*>(buffer.data()), n, 0);
            if (sent <= 0) break;
            m_bytesIn += sent;
        }

        closed.store(true);
        closesocket(clientSock);
        closesocket(remoteSock);
    });

    tUpstream.join();
    tDownstream.join();
}

} // namespace AxeDpi
