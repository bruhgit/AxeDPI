#include <windows.h>
#define AXEDPI_EXPORTS
#include "dll_exports.h"
#include "dpi_engine.h"
#include "sysproxy.h"
#include "doh_resolver.h"
#include <cstring>

static DpiLogCallback g_logCallback = nullptr;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        break;
    case DLL_PROCESS_DETACH:
        if (AxeDpi::DpiEngine::Instance().IsRunning()) {
            AxeDpi::DpiEngine::Instance().Stop();
        }
        break;
    }
    return TRUE;
}

static void ForwardLog(const std::string& msg) {
    if (g_logCallback) {
        g_logCallback(msg.c_str());
    }
}

int __cdecl DpiStartServer(const char* bindHost, int bindPort, int splitPos, int splitDelayMs, const char* upstreamProxy, int autoSysProxy) {
    AxeDpi::DpiConfig cfg;
    if (bindHost && std::strlen(bindHost) > 0) {
        cfg.bindHost = bindHost;
    } else {
        cfg.bindHost = "127.0.0.1";
    }

    cfg.bindPort = (bindPort > 0) ? bindPort : 10808;
    cfg.splitPos = (splitPos > 0) ? splitPos : 40;
    cfg.splitDelayMs = (splitDelayMs >= 0) ? splitDelayMs : 10;
    if (upstreamProxy) {
        cfg.upstreamProxy = upstreamProxy;
    }

    AxeDpi::DpiEngine::Instance().SetLogCallback(ForwardLog);

    bool ok = AxeDpi::DpiEngine::Instance().Start(cfg);
    if (ok && autoSysProxy) {
        std::string fullAddr = cfg.bindHost + ":" + std::to_string(cfg.bindPort);
        AxeDpi::SysProxyManager::SetSystemProxy(true, fullAddr);
    }
    return ok ? 1 : 0;
}

void __cdecl DpiStopServer() {
    if (AxeDpi::SysProxyManager::IsSystemProxyEnabled()) {
        AxeDpi::SysProxyManager::SetSystemProxy(false, "");
    }
    AxeDpi::DpiEngine::Instance().Stop();
}

int __cdecl DpiIsRunning() {
    return AxeDpi::DpiEngine::Instance().IsRunning() ? 1 : 0;
}

int __cdecl DpiSetSystemProxy(int enable, const char* proxyAddr) {
    std::string addr = proxyAddr ? proxyAddr : "127.0.0.1:10808";
    return AxeDpi::SysProxyManager::SetSystemProxy(enable != 0, addr) ? 1 : 0;
}

int __cdecl DpiIsSystemProxyEnabled() {
    return AxeDpi::SysProxyManager::IsSystemProxyEnabled() ? 1 : 0;
}

int __cdecl DpiSetStartup(int enable, const char* exePath) {
    std::string path = exePath ? exePath : "";
    return AxeDpi::SysProxyManager::SetStartup(enable != 0, path) ? 1 : 0;
}

int __cdecl DpiIsStartupEnabled() {
    return AxeDpi::SysProxyManager::IsStartupEnabled() ? 1 : 0;
}

int __cdecl DpiLaunchDiscordWithProxy(const char* proxyAddr) {
    std::string addr = proxyAddr ? proxyAddr : "127.0.0.1:10808";
    return AxeDpi::SysProxyManager::LaunchDiscordWithProxy(addr) ? 1 : 0;
}

int __cdecl DpiTestDoH(const char* testDomain, char* outIpBuffer, int bufferLen, int* outLatencyMs) {
    std::string domain = (testDomain && std::strlen(testDomain) > 0) ? testDomain : "discord.com";
    auto resp = AxeDpi::DohResolver::Instance().Resolve(domain, true);

    if (outLatencyMs) {
        *outLatencyMs = resp.latencyMs;
    }

    if (resp.success && !resp.ips.empty()) {
        if (outIpBuffer && bufferLen > 0) {
            strncpy_s(outIpBuffer, bufferLen, resp.ips[0].c_str(), _TRUNCATE);
        }
        return 1;
    }

    if (outIpBuffer && bufferLen > 0) {
        strncpy_s(outIpBuffer, bufferLen, "Resolution Failed", _TRUNCATE);
    }
    return 0;
}

void __cdecl DpiGetStats(unsigned long long* bytesIn, unsigned long long* bytesOut, int* activeConnections) {
    uint64_t inB = 0, outB = 0;
    int conns = 0;
    AxeDpi::DpiEngine::Instance().GetStats(inB, outB, conns);

    if (bytesIn) *bytesIn = inB;
    if (bytesOut) *bytesOut = outB;
    if (activeConnections) *activeConnections = conns;
}

void __cdecl DpiSetLogCallback(DpiLogCallback callback) {
    g_logCallback = callback;
    AxeDpi::DpiEngine::Instance().SetLogCallback(ForwardLog);
}
