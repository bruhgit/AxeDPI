@echo off
setlocal
echo ===================================================================
echo                     AxeDPI - Build Pipeline
echo ===================================================================

cd /d "%~dp0core_cpp"
echo [1/2] C++ DPI Motoru (AxeDPI.Service.DllComponents.dll) derleniyor...
call build_dll.bat
if %ERRORLEVEL% neq 0 (
    echo [HATA] C++ DLL derleme basarisiz oldu!
    pause
    exit /b 1
)

cd /d "%~dp0AxeDPI.UI"
echo.
echo [2/2] C# WPF Arayuzu derleniyor ve paketleniyor...
dotnet publish -c Release -o ../publish
if %ERRORLEVEL% neq 0 (
    echo [HATA] WPF arayuz derlemesi basarisiz oldu!
    pause
    exit /b 1
)

if exist "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe" (
    echo.
    echo [3/3] Inno Setup Kurulum Paketi (AxeDPI_Setup_v2.0.exe) derleniyor...
    "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe" "%~dp0installer\AxeDPI_Setup.iss" >nul
    echo [BASARILI] Kurulum paketi: installer_output\AxeDPI_Setup_v2.0.exe
)

echo.
echo ===================================================================
echo [BASARILI] AxeDPI Remaked basariyla derlendi!
echo Calistirmak icin: publish\AxeDPI.UI.exe veya BASLAT.bat
echo Kurulum paketi: installer_output\AxeDPI_Setup_v2.0.exe
echo ===================================================================
pause
endlocal
