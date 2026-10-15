#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <cstdint>

namespace AxeDpi {

struct DpiConfig {
    std::string bindHost = "127.0.0.1";
    int bindPort = 10808;
    int splitPos = 40;
    int splitDelayMs = 10;
    std::string upstreamProxy = "";
    bool verbose = true;
};

class DpiEngine {
public:
    static DpiEngine& Instance();

    bool Start(const DpiConfig& config);
    void Stop();
    bool IsRunning() const;

    void GetStats(uint64_t& outBytesIn, uint64_t& outBytesOut, int& outActiveConns) const;
    void SetLogCallback(std::function<void(const std::string&)> cb);
    void Log(const std::string& msg);

private:
    DpiEngine();
    ~DpiEngine();

    void ServerLoop();
    void HandleClient(uintptr_t clientSockRaw);
    void HandleSocks5(uintptr_t clientSockRaw, const uint8_t* initialByte);
    void HandleHttp(uintptr_t clientSockRaw, const std::string& initialLine);

    uintptr_t DialTarget(const std::string& host, int port);
    void TunnelWithDpi(uintptr_t clientSockRaw, uintptr_t remoteSockRaw);

    DpiConfig m_config;
    std::atomic<bool> m_running{false};
    uintptr_t m_listenSock{0}; // SOCKET
    std::thread m_serverThread;

    std::atomic<uint64_t> m_bytesIn{0};
    std::atomic<uint64_t> m_bytesOut{0};
    std::atomic<int> m_activeConns{0};

    std::function<void(const std::string&)> m_logCallback;
};

} // namespace AxeDpi
