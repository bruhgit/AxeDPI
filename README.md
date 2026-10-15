# AxeDPI

A high-performance Windows Deep Packet Inspection (DPI) circumvention proxy featuring a native C++ engine and a minimalist WPF control interface.

---

## Overview

AxeDPI is designed to bypass protocol-level filtering, SNI-based blocking, and DNS tampering implemented by Internet Service Providers (ISPs) and corporate firewalls. It operates locally as a transparent proxy, intercepting outgoing TLS handshakes and HTTP requests to segment sensitive packet headers before they hit inspecting middleboxes.

The application is structured into two distinct decoupled tiers:
- **Core Engine (`AxeDPI.Service.DllComponents.dll`)**: Written in modern C++17, utilizing asynchronous Winsock2 sockets, TLS handshake byte-stream parsing, and WinHTTP-based DNS-over-HTTPS (DoH).
- **Control Interface (`AxeDPI.UI.exe`)**: Written in C# with WPF on .NET 9.0, implementing MVVM architecture, single-instance process mutual exclusion, Windows system tray integration, and custom vector rendering.

---

## Technical Architecture

```
AxeDPI/
├── core_cpp/                           # Native C++ DPI Engine
│   ├── include/
│   │   ├── dll_exports.h               # C-ABI export interface
│   │   ├── dpi_engine.h                # Socket server and tunneling pool
│   │   ├── sni_parser.h                # TLS Client Hello & SNI packet parser
│   │   ├── doh_resolver.h              # RFC 8484 DNS-over-HTTPS client
│   │   └── sysproxy.h                  # WinINet and Windows Registry hooks
│   ├── src/
│   │   ├── dll_main.cpp                # DLL entry point and lifetime management
│   │   ├── dpi_engine.cpp              # SOCKS5 & HTTP CONNECT multiplexer
│   │   ├── sni_parser.cpp              # Dynamic TLS record fragmentation
│   │   ├── doh_resolver.cpp            # Cloudflare & Google DoH resolver
│   │   └── sysproxy.cpp                # System proxy and process launcher
│   ├── CMakeLists.txt                  # CMake build configuration
│   └── build_dll.bat                   # Standalone MinGW compilation script
│
├── AxeDPI.UI/                          # C# WPF Desktop Control Interface
│   ├── Services/
│   │   ├── DpiService.cs               # Native interop wrapper and lifecycle
│   │   └── TrayService.cs              # System tray (NotifyIcon) integration
│   ├── ViewModels/
│   │   ├── MainViewModel.cs            # State management and UI commands
│   │   ├── RelayCommand.cs             # ICommand delegate implementation
│   │   └── ViewModelBase.cs            # INotifyPropertyChanged base
│   ├── MainWindow.xaml                 # Pure pitch-black interface layout
│   ├── MainWindow.xaml.cs              # Window chrome and event handlers
│   ├── App.xaml                        # Application themes and geometry resources
│   ├── App.xaml.cs                     # Single-instance Mutex & IPC signal handling
│   └── app.manifest                    # Per-Monitor V2 DPI awareness manifest
│
├── installer/                          # Inno Setup 6 packaging
│   ├── AxeDPI_Setup.iss                # Setup script definition
│   └── KURUCU_DERLE.bat                # Automated installer compiler
│
├── tests/                              # Diagnostic and test utilities
│   └── dll_test.cpp                    # Native C-ABI unit test suite
│
├── AxeDPI.Service.DllComponents.dll    # Prebuilt static C++ core engine
├── DERLE.bat                           # Unified build pipeline script
├── BASLAT.bat                          # Application launch runner
└── LICENSE                             # MIT License
```

---

## Core Capabilities

### 1. TLS SNI Record Segmentation
Middleboxes and DPI firewalls inspect the initial unencrypted TLS `Client Hello` packet to read the `server_name` (SNI) extension and block blacklisted domains. 
AxeDPI inspects incoming byte streams (`0x16 0x03 0x01..0x04`), parses the TLS extension tree to calculate the precise byte offset of the SNI hostname, and splits the record into two distinct TCP segments sent with microsecond-level timing delays (`TCP_NODELAY`). Because inspecting firewalls rely on naive single-packet reassembly, they fail to reconstruct the full SNI string, while remote TLS termination endpoints (Cloudflare, Google, Discord, etc.) reconstruct the full stream normally.

### 2. HTTP Header Normalization & Mutation
For unencrypted HTTP traffic, AxeDPI fragments the request line and alters header casing (such as rewriting `Host:` to `hOsT:`), disrupting regular-expression pattern matchers used by standard DPI hardware.

### 3. Integrated DNS-over-HTTPS (DoH)
To prevent ISP DNS poisoning and false NXDOMAIN responses, AxeDPI includes an in-memory cached RFC 8484 DoH resolver using Cloudflare (`1.1.1.1`) and Google (`8.8.8.8`) endpoints via Windows HTTP services (`winhttp.dll`).

### 4. Live DPI Pause / Resume Mode
Allows users to pause DPI manipulation on-the-fly via UI or system tray without disconnecting or restarting the proxy service. Outbound sockets on `127.0.0.1:10808` stay active, passing packets through directly without TLS segmentation or delay, preventing connection drops in web browsers.

### 5. Process-Level Exclusions List
Uses the Windows IP Helper API (`GetExtendedTcpTable` + `QueryFullProcessImageNameW`) to resolve the client process name for each incoming TCP connection. Connections originating from user-defined executables (e.g., `spotify.exe`, `riotclient.exe`, `valorant.exe`) bypass TLS fragmentation entirely, preventing conflicts with anti-cheat software and strict TLS pinning.

### 6. Windows System Proxy Hook
Allows one-click system-wide routing via WinINet API (`INTERNET_OPTION_SETTINGS_CHANGED`), configuring Windows Registry proxy keys automatically without requiring manual network adapter configuration.

### 7. Discord Proxy Integration
Detects local Discord installations across `%LOCALAPPDATA%` and launches Discord instances configured with `--proxy-server` targeting the local AxeDPI port, allowing uninterrupted Voice/Gateway connections through restrictive networks.

---

## C-ABI Export Interface

The core C++ engine exports clean standard C ABI functions, enabling integration with any language runtime (C#, Python, Rust, Go):

```c
int  DpiStartServer(const char* bindHost, int bindPort, int splitPos, int splitDelayMs, const char* upstreamProxy, int autoSysProxy);
void DpiStopServer();
int  DpiIsRunning();
void DpiSetPaused(int paused);
int  DpiIsPaused();
void DpiSetExcludedApps(const char* processList);
int  DpiIsAppExcluded(const char* processName);
int  DpiSetSystemProxy(int enable, const char* proxyAddr);
int  DpiIsSystemProxyEnabled();
int  DpiSetStartup(int enable, const char* exePath);
int  DpiIsStartupEnabled();
int  DpiLaunchDiscordWithProxy(const char* proxyAddr);
int  DpiTestDoH(const char* testDomain, char* outIpBuffer, int bufferLen, int* outLatencyMs);
void DpiGetStats(unsigned long long* bytesIn, unsigned long long* bytesOut, int* activeConnections);
void DpiSetLogCallback(void (__stdcall *callback)(const char* message));
```

---

## Building from Source

### Prerequisites
- **Operating System**: Windows 10 / Windows 11 (x64)
- **.NET SDK**: .NET 9.0 SDK or later
- **C++ Compiler**: MinGW-w64 (GCC 13+ with win32/posix threads) or MSVC
- **Packaging (Optional)**: Inno Setup 6.x (for building the installer executable)

### 1. Build the C++ Core Engine
```cmd
cd core_cpp
build_dll.bat
```
This produces `AxeDPI.Service.DllComponents.dll` with static linkage (`-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic`).

### 2. Build the WPF Application
```cmd
cd AxeDPI.UI
dotnet publish -c Release -o ../publish
```

### 3. Compile Installer (Optional)
```cmd
cd installer
KURUCU_DERLE.bat
```

Or execute the root automated pipeline script:
```cmd
DERLE.bat
```

---

## License

This project is licensed under the [MIT License](LICENSE).
