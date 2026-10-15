#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AXEDPI_EXPORTS
#define AXEDPI_API __declspec(dllexport)
#else
#define AXEDPI_API __declspec(dllimport)
#endif

// Server Lifecycle
AXEDPI_API int __cdecl DpiStartServer(const char* bindHost, int bindPort, int splitPos, int splitDelayMs, const char* upstreamProxy, int autoSysProxy);
AXEDPI_API void __cdecl DpiStopServer();
AXEDPI_API int __cdecl DpiIsRunning();

// System Proxy
AXEDPI_API int __cdecl DpiSetSystemProxy(int enable, const char* proxyAddr);
AXEDPI_API int __cdecl DpiIsSystemProxyEnabled();

// Windows Startup
AXEDPI_API int __cdecl DpiSetStartup(int enable, const char* exePath);
AXEDPI_API int __cdecl DpiIsStartupEnabled();

// Discord Launcher
AXEDPI_API int __cdecl DpiLaunchDiscordWithProxy(const char* proxyAddr);

// Diagnostics & DoH Test
AXEDPI_API int __cdecl DpiTestDoH(const char* testDomain, char* outIpBuffer, int bufferLen, int* outLatencyMs);

// Statistics (Traffic throughput & active connections)
AXEDPI_API void __cdecl DpiGetStats(unsigned long long* bytesIn, unsigned long long* bytesOut, int* activeConnections);

// Logging
typedef void (__stdcall *DpiLogCallback)(const char* message);
AXEDPI_API void __cdecl DpiSetLogCallback(DpiLogCallback callback);

#ifdef __cplusplus
}
#endif
