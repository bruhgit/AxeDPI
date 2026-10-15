#include "doh_resolver.h"
#include <windows.h>
#include <winhttp.h>
#include <mutex>
#include <unordered_map>
#include <regex>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace AxeDpi {

static std::unordered_map<std::string, DohResolver::CacheEntry> s_cache;
static std::mutex s_cacheMutex;

DohResolver::DohResolver() {}
DohResolver::~DohResolver() {}

DohResolver& DohResolver::Instance() {
    static DohResolver s_instance;
    return s_instance;
}

void DohResolver::ClearCache() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_cache.clear();
}

std::vector<std::string> DohResolver::ParseDnsJsonAnswers(const std::string& json) {
    std::vector<std::string> ips;

    // Fast and robust regex matching for IPv4 data in DNS JSON response
    // E.g., "type":1, ... "data":"1.2.3.4" or "data":"104.16.132.229"
    // Also handling escaped quotes or spaces
    std::regex answerRegex(R"(\"type\"\s*:\s*1\s*,[^\}]*?\"data\"\s*:\s*\"([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})\")");
    auto words_begin = std::sregex_iterator(json.begin(), json.end(), answerRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string ip = match[1].str();
        ips.push_back(ip);
    }

    if (ips.empty()) {
        // Alternative field order: "data":"1.2.3.4", ... "type":1
        std::regex altRegex(R"(\"data\"\s*:\s*\"([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})\"[^\}]*?\"type\"\s*:\s*1)");
        auto alt_begin = std::sregex_iterator(json.begin(), json.end(), altRegex);
        for (std::sregex_iterator i = alt_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string ip = match[1].str();
            ips.push_back(ip);
        }
    }

    return ips;
}

static std::string WinHttpGet(const std::wstring& host, INTERNET_PORT port, const std::wstring& path, const std::wstring& headers) {
    HINTERNET hSession = WinHttpOpen(L"AxeDPI-DoH/2.0",
                                    WINHTTP_ACCESS_TYPE_NO_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    // Set timeout to 3.5 seconds
    WinHttpSetTimeouts(hSession, 3000, 3000, 3500, 3500);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Ignore SSL certificate CN errors when connecting directly to IP address (1.1.1.1 or 8.8.8.8)
    DWORD dwFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                    SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));

    BOOL bResults = WinHttpSendRequest(hRequest,
                                      headers.c_str(),
                                      static_cast<DWORD>(headers.length()),
                                      WINHTTP_NO_REQUEST_DATA, 0,
                                      0, 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    std::string responseBody;
    if (bResults) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                std::vector<char> buffer(dwSize + 1, 0);
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    responseBody.append(buffer.data(), dwDownloaded);
                }
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return responseBody;
}

DohResponse DohResolver::QueryCloudflare(const std::string& hostname) {
    DohResponse res;
    res.success = false;
    res.latencyMs = 0;

    auto start = std::chrono::steady_clock::now();
    std::wstring path = L"/dns-query?name=" + std::wstring(hostname.begin(), hostname.end()) + L"&type=A";
    std::wstring headers = L"Accept: application/dns-json\r\nHost: cloudflare-dns.com\r\n";

    std::string json = WinHttpGet(L"1.1.1.1", INTERNET_DEFAULT_HTTPS_PORT, path, headers);
    auto end = std::chrono::steady_clock::now();
    res.latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    if (!json.empty()) {
        res.ips = ParseDnsJsonAnswers(json);
        if (!res.ips.empty()) {
            res.success = true;
            return res;
        }
    }

    res.errorMessage = "Cloudflare DoH returned empty response or invalid JSON";
    return res;
}

DohResponse DohResolver::QueryGoogle(const std::string& hostname) {
    DohResponse res;
    res.success = false;
    res.latencyMs = 0;

    auto start = std::chrono::steady_clock::now();
    std::wstring path = L"/resolve?name=" + std::wstring(hostname.begin(), hostname.end()) + L"&type=A";
    std::wstring headers = L"Accept: application/json\r\nHost: dns.google\r\n";

    std::string json = WinHttpGet(L"8.8.8.8", INTERNET_DEFAULT_HTTPS_PORT, path, headers);
    auto end = std::chrono::steady_clock::now();
    res.latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    if (!json.empty()) {
        res.ips = ParseDnsJsonAnswers(json);
        if (!res.ips.empty()) {
            res.success = true;
            return res;
        }
    }

    res.errorMessage = "Google DoH returned empty response or invalid JSON";
    return res;
}

DohResponse DohResolver::Resolve(const std::string& hostname, bool verbose) {
    // 1. Check in-memory cache
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_cache.find(hostname);
        if (it != s_cache.end() && it->second.expiresAt > now) {
            DohResponse cached;
            cached.success = true;
            cached.ips = it->second.ips;
            cached.latencyMs = 0;
            return cached;
        }
    }

    // 2. Query Cloudflare (1.1.1.1)
    DohResponse cf = QueryCloudflare(hostname);
    if (cf.success && !cf.ips.empty()) {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_cache[hostname] = {cf.ips, now + std::chrono::minutes(10)};
        return cf;
    }

    // 3. Fallback to Google (8.8.8.8)
    DohResponse gg = QueryGoogle(hostname);
    if (gg.success && !gg.ips.empty()) {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_cache[hostname] = {gg.ips, now + std::chrono::minutes(10)};
        return gg;
    }

    // 4. Return failure
    DohResponse failed;
    failed.success = false;
    failed.latencyMs = cf.latencyMs + gg.latencyMs;
    failed.errorMessage = "Failed to resolve via Cloudflare and Google DoH";
    return failed;
}

} // namespace AxeDpi
