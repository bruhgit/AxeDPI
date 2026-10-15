@echo off
setlocal enabledelayedexpansion

echo ===================================================================
echo               AxeDPI - Inno Setup Kurucu Derleyici
echo ===================================================================

set "ISCC="

REM 1. Check in PATH
where iscc >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set "ISCC=iscc"
    goto Compile
)

REM 2. Check standard Inno Setup 6 install directories
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
    set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
    goto Compile
)

if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" (
    set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
    goto Compile
)

if exist "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe" (
    set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
    goto Compile
)

echo [BILGI] Sistemde Inno Setup 6 bulunamadi.
echo Inno Setup 6'yi yuklemek icin https://jrsoftware.org/isdl.php adresinden
echo innosetup-6.x.x.exe dosyasini indirip kurabilirsiniz.
echo.
pause
exit /b 1

:Compile
echo [1/2] Inno Setup derleyicisi bulundu: "%ISCC%"
echo [2/2] AxeDPI_Setup.iss derleniyor...
echo.

"%ISCC%" "%~dp0AxeDPI_Setup.iss"
if %ERRORLEVEL% neq 0 (
    echo.
    echo [HATA] Kurucu olusturma basarisiz oldu!
    pause
    exit /b 1
)

echo.
echo ===================================================================
echo [BASARILI] AxeDPI Kurulum Dosyasi Olusturuldu!
echo Konum: installer_output\AxeDPI_Setup_v2.0.exe
echo ===================================================================
pause
endlocal
