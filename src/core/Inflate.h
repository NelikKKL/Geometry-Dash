// Dependency-free decoders for level strings: base64 (standard + URL-safe), DEFLATE (RFC 1951),
// gzip (RFC 1952, CRC-checked) and zlib (RFC 1950, Adler-checked).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ogd {

// Accepts both alphabets ('+/' and '-_'), ignores whitespace, tolerates missing '=' padding.
bool base64Decode(const std::string& in, std::vector<uint8_t>& out);

constexpr size_t kDefaultMaxInflate = 64u << 20;  // decompression-bomb guard

bool inflateRaw(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut = kDefaultMaxInflate);
bool gunzip(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut = kDefaultMaxInflate);
bool zlibDecompress(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut = kDefaultMaxInflate);

// Detects gzip (1f 8b) / zlib (78 xx) / raw by header. Returns false on corrupt data.
bool decompressAuto(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut = kDefaultMaxInflate);

uint32_t crc32(const uint8_t* data, size_t size);

} // namespace ogd
