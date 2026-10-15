#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace AxeDpi {

struct SniResult {
    bool found;
    int offset; // Relative to handshake payload (after 5-byte record header)
    int length;
    std::string hostname;
};

// Checks if buffer starts with TLS Client Hello (record type 0x16, version 0x03 0x01..03, handshake type 0x01)
bool IsTlsClientHello(const uint8_t* data, size_t len);

// Checks if buffer starts with standard HTTP methods (GET, POST, etc.)
bool IsHttpGetOrPost(const uint8_t* data, size_t len);

// Parses TLS Client Hello payload (data after 5-byte TLS record header) and locates SNI extension
SniResult FindSniOffset(const uint8_t* handshakePayload, size_t payloadLen);

// Modifies HTTP request headers (e.g. changing "Host:" to "hOsT:") to bypass naive DPI filters
std::vector<uint8_t> ModifyHttpHeaders(const uint8_t* data, size_t len);

} // namespace AxeDpi
