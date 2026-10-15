#include "sni_parser.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace AxeDpi {

bool IsTlsClientHello(const uint8_t* data, size_t len) {
    if (len < 6) return false;
    // TLS Record Header:
    // Byte 0: Content Type: 0x16 (Handshake)
    // Byte 1-2: Version: 0x03 0x01 through 0x03 0x04
    // Byte 5: Handshake Type: 0x01 (Client Hello)
    return (data[0] == 0x16 &&
            data[1] == 0x03 &&
            data[2] >= 0x01 && data[2] <= 0x04 &&
            data[5] == 0x01);
}

bool IsHttpGetOrPost(const uint8_t* data, size_t len) {
    if (len < 4) return false;
    const char* methods[] = {"GET ", "POST ", "HEAD ", "PUT ", "DELETE ", "OPTIONS ", "PATCH "};
    for (const char* m : methods) {
        size_t mlen = std::strlen(m);
        if (len >= mlen && std::memcmp(data, m, mlen) == 0) {
            return true;
        }
    }
    return false;
}

static inline uint16_t ReadUint16BE(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

SniResult FindSniOffset(const uint8_t* handshakePayload, size_t payloadLen) {
    SniResult result = {false, -1, 0, ""};
    if (payloadLen < 42 || handshakePayload[0] != 0x01) {
        return result;
    }

    // 4 (type + len) + 2 (version) + 32 (random) = 38
    size_t offset = 38;
    if (offset >= payloadLen) return result;

    // Session ID
    size_t sessIdLen = handshakePayload[offset];
    offset += 1 + sessIdLen;
    if (offset + 2 > payloadLen) return result;

    // Cipher Suites
    size_t cipherLen = ReadUint16BE(handshakePayload + offset);
    offset += 2 + cipherLen;
    if (offset + 1 > payloadLen) return result;

    // Compression Methods
    size_t compLen = handshakePayload[offset];
    offset += 1 + compLen;
    if (offset + 2 > payloadLen) return result;

    // Extensions Total Length
    size_t extTotalLen = ReadUint16BE(handshakePayload + offset);
    offset += 2;
    size_t extEnd = offset + extTotalLen;
    if (extEnd > payloadLen) extEnd = payloadLen;

    while (offset + 4 <= extEnd) {
        uint16_t extType = ReadUint16BE(handshakePayload + offset);
        uint16_t extLen = ReadUint16BE(handshakePayload + offset + 2);
        offset += 4;

        if (offset + extLen > extEnd) break;

        if (extType == 0x0000) { // Server Name Indication (SNI)
            const uint8_t* extData = handshakePayload + offset;
            if (extLen >= 5) {
                uint8_t nameType = extData[2];
                if (nameType == 0x00) { // host_name
                    uint16_t nameLen = ReadUint16BE(extData + 3);
                    if (extLen >= 5 + nameLen) {
                        result.found = true;
                        result.offset = static_cast<int>(offset + 5);
                        result.length = static_cast<int>(nameLen);
                        result.hostname = std::string(reinterpret_cast<const char*>(extData + 5), nameLen);
                        return result;
                    }
                }
            }
        }
        offset += extLen;
    }

    return result;
}

std::vector<uint8_t> ModifyHttpHeaders(const uint8_t* data, size_t len) {
    std::string str(reinterpret_cast<const char*>(data), len);
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const std::string hostPrefix = "\r\nhost:";
    size_t pos = lowerStr.find(hostPrefix);
    if (pos != std::string::npos) {
        // Replace "host:" with "hOsT:"
        str.replace(pos + 2, 4, "hOsT");
    } else if (lowerStr.rfind("host:", 0) == 0) {
        str.replace(0, 4, "hOsT");
    }

    return std::vector<uint8_t>(str.begin(), str.end());
}

} // namespace AxeDpi
