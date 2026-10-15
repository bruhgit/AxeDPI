#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace AxeDpi {

struct DohResponse {
    bool success;
    std::vector<std::string> ips;
    int latencyMs;
    std::string errorMessage;
};

class DohResolver {
public:
    static DohResolver& Instance();

    // Resolves a hostname to a list of IPv4 addresses using DoH (Cloudflare / Google)
    DohResponse Resolve(const std::string& hostname, bool verbose = false);

    // Clears the internal DNS cache
    void ClearCache();

private:
    DohResolver();
    ~DohResolver();

public:
    struct CacheEntry {
        std::vector<std::string> ips;
        std::chrono::steady_clock::time_point expiresAt;
    };

    DohResponse QueryCloudflare(const std::string& hostname);
    DohResponse QueryGoogle(const std::string& hostname);
    static std::vector<std::string> ParseDnsJsonAnswers(const std::string& json);
};

} // namespace AxeDpi
