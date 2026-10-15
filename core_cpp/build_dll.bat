@echo off
setlocal
echo ========================================================
echo       Building AxeDPI.Service.DllComponents.dll
echo ========================================================

where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] g++ not found in PATH!
    pause
    exit /b 1
)

g++ -shared -O3 -std=c++17 -I./include src/dll_main.cpp src/dpi_engine.cpp src/sni_parser.cpp src/doh_resolver.cpp src/sysproxy.cpp -o ../AxeDPI.Service.DllComponents.dll -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic -lws2_32 -lwininet -lwinhttp -liphlpapi
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo [SUCCESS] AxeDPI.Service.DllComponents.dll built successfully!
endlocal
