#pragma once

#include <string>

namespace AxeDpi {

class SysProxyManager {
public:
    static bool SetSystemProxy(bool enable, const std::string& proxyAddr);
    static bool IsSystemProxyEnabled();

    static bool SetStartup(bool enable, const std::string& exePath);
    static bool IsStartupEnabled();

    static bool LaunchDiscordWithProxy(const std::string& proxyAddr);
};

} // namespace AxeDpi
