#include "sysproxy.h"
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>

#pragma comment(lib, "wininet.lib")

namespace AxeDpi {

bool SysProxyManager::SetSystemProxy(bool enable, const std::string& proxyAddr) {
    HKEY hKey;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                            0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    if (enable) {
        DWORD dwEnable = 1;
        RegSetValueExA(hKey, "ProxyEnable", 0, REG_DWORD, (const BYTE*)&dwEnable, sizeof(dwEnable));
        RegSetValueExA(hKey, "ProxyServer", 0, REG_SZ, (const BYTE*)proxyAddr.c_str(), static_cast<DWORD>(proxyAddr.length() + 1));
        const char* overrideHosts = "localhost;127.*;10.*;172.16.*;192.168.*;<local>";
        RegSetValueExA(hKey, "ProxyOverride", 0, REG_SZ, (const BYTE*)overrideHosts, static_cast<DWORD>(strlen(overrideHosts) + 1));
    } else {
        DWORD dwEnable = 0;
        RegSetValueExA(hKey, "ProxyEnable", 0, REG_DWORD, (const BYTE*)&dwEnable, sizeof(dwEnable));
        RegDeleteValueA(hKey, "ProxyServer");
        RegDeleteValueA(hKey, "ProxyOverride");
    }

    RegCloseKey(hKey);

    // Refresh WinINet system settings immediately
    InternetSetOptionA(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionA(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    return true;
}

bool SysProxyManager::IsSystemProxyEnabled() {
    HKEY hKey;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                            0, KEY_QUERY_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    DWORD dwType = 0;
    DWORD dwValue = 0;
    DWORD dwSize = sizeof(dwValue);
    res = RegQueryValueExA(hKey, "ProxyEnable", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
    RegCloseKey(hKey);

    return (res == ERROR_SUCCESS && dwValue == 1);
}

bool SysProxyManager::SetStartup(bool enable, const std::string& exePath) {
    HKEY hKey;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            0, KEY_SET_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    if (enable) {
        std::string path = exePath;
        if (path.empty()) {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            path = buffer;
        }
        std::string quoted = "\"" + path + "\"";
        res = RegSetValueExA(hKey, "AxeDPI", 0, REG_SZ, (const BYTE*)quoted.c_str(), static_cast<DWORD>(quoted.length() + 1));
    } else {
        res = RegDeleteValueA(hKey, "AxeDPI");
    }

    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS);
}

bool SysProxyManager::IsStartupEnabled() {
    HKEY hKey;
    LONG res = RegOpenKeyExA(HKEY_CURRENT_USER,
                            "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                            0, KEY_QUERY_VALUE, &hKey);
    if (res != ERROR_SUCCESS) return false;

    char buffer[MAX_PATH];
    DWORD dwSize = sizeof(buffer);
    LONG queryRes = RegQueryValueExA(hKey, "AxeDPI", NULL, NULL, (LPBYTE)buffer, &dwSize);
    RegCloseKey(hKey);

    return (queryRes == ERROR_SUCCESS);
}

static void KillProcessByName(const std::wstring& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

bool SysProxyManager::LaunchDiscordWithProxy(const std::string& proxyAddr) {
    char localAppData[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;

    std::filesystem::path discordBase = std::filesystem::path(localAppData) / "Discord";
    if (!std::filesystem::exists(discordBase)) return false;

    std::filesystem::path latestAppDir;
    for (const auto& entry : std::filesystem::directory_iterator(discordBase)) {
        if (entry.is_directory()) {
            std::string dirName = entry.path().filename().string();
            if (dirName.rfind("app-", 0) == 0) {
                if (latestAppDir.empty() || dirName > latestAppDir.filename().string()) {
                    latestAppDir = entry.path();
                }
            }
        }
    }

    if (latestAppDir.empty()) return false;

    std::filesystem::path discordExe = latestAppDir / "Discord.exe";
    if (!std::filesystem::exists(discordExe)) return false;

    // Terminate existing Discord instances
    KillProcessByName(L"Discord.exe");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Launch Discord with proxy parameter
    std::string cmdLine = "\"" + discordExe.string() + "\" --proxy-server=http://" + proxyAddr;

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    std::vector<char> cmdVec(cmdLine.begin(), cmdLine.end());
    cmdVec.push_back('\0');

    BOOL created = CreateProcessA(NULL, cmdVec.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (created) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

    return false;
}

} // namespace AxeDpi
