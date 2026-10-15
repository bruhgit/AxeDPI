#include <windows.h>
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

typedef int (__cdecl *FnDpiStartServer)(const char*, int, int, int, const char*, int);
typedef void (__cdecl *FnDpiStopServer)();
typedef int (__cdecl *FnDpiIsRunning)();
typedef int (__cdecl *FnDpiTestDoH)(const char*, char*, int, int*);
typedef void (__cdecl *FnDpiGetStats)(unsigned long long*, unsigned long long*, int*);
typedef void (__cdecl *FnDpiSetLogCallback)(void (__stdcall *)(const char*));

void __stdcall TestLogCallback(const char* msg) {
    std::cout << "[NATIVE LOG] " << msg << std::endl;
}

int main() {
    std::cout << "=== Testing AxeDPI.Service.DllComponents.dll ===" << std::endl;

    HMODULE hDll = LoadLibraryA("AxeDPI.Service.DllComponents.dll");
    if (!hDll) {
        std::cerr << "Failed to load DLL! Error: " << GetLastError() << std::endl;
        return 1;
    }
    std::cout << "[PASS] DLL loaded successfully." << std::endl;

    auto DpiStartServer = (FnDpiStartServer)GetProcAddress(hDll, "DpiStartServer");
    auto DpiStopServer = (FnDpiStopServer)GetProcAddress(hDll, "DpiStopServer");
    auto DpiIsRunning = (FnDpiIsRunning)GetProcAddress(hDll, "DpiIsRunning");
    auto DpiTestDoH = (FnDpiTestDoH)GetProcAddress(hDll, "DpiTestDoH");
    auto DpiGetStats = (FnDpiGetStats)GetProcAddress(hDll, "DpiGetStats");
    auto DpiSetLogCallback = (FnDpiSetLogCallback)GetProcAddress(hDll, "DpiSetLogCallback");

    if (!DpiStartServer || !DpiStopServer || !DpiIsRunning || !DpiTestDoH || !DpiGetStats || !DpiSetLogCallback) {
        std::cerr << "Failed to locate all exported functions!" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }
    std::cout << "[PASS] All exported functions located." << std::endl;

    DpiSetLogCallback(TestLogCallback);

    // Test DoH
    std::cout << "[TEST] Testing DoH resolution for discord.com..." << std::endl;
    char ipBuf[128] = {0};
    int latency = 0;
    int dohRes = DpiTestDoH("discord.com", ipBuf, sizeof(ipBuf), &latency);
    std::cout << "DoH Result: " << dohRes << ", IP: " << ipBuf << ", Latency: " << latency << "ms" << std::endl;
    if (dohRes != 1) {
        std::cerr << "[FAIL] DoH resolution failed!" << std::endl;
    } else {
        std::cout << "[PASS] DoH resolved successfully!" << std::endl;
    }

    // Test Start Server
    std::cout << "[TEST] Starting DPI Proxy Server on 127.0.0.1:19898..." << std::endl;
    int startRes = DpiStartServer("127.0.0.1", 19898, 40, 10, NULL, 0);
    std::cout << "Start result: " << startRes << std::endl;
    assert(startRes == 1);
    assert(DpiIsRunning() == 1);
    std::cout << "[PASS] Server is running." << std::endl;

    // Check stats
    unsigned long long inB = 0, outB = 0;
    int conns = 0;
    DpiGetStats(&inB, &outB, &conns);
    std::cout << "[PASS] Stats queried: In=" << inB << ", Out=" << outB << ", Conns=" << conns << std::endl;

    // Test Stop Server
    std::cout << "[TEST] Stopping server..." << std::endl;
    DpiStopServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(DpiIsRunning() == 0);
    std::cout << "[PASS] Server stopped cleanly." << std::endl;

    FreeLibrary(hDll);
    std::cout << "\n>>> ALL NATIVE DLL COMPONENT TESTS PASSED! <<<" << std::endl;
    return 0;
}
