#include "core/Inflate.h"

#include <cstring>

namespace ogd {
namespace {

// ------------------------------------------------------------------ base64
int b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

// ----------------------------------------------------------------- inflate
// Canonical-Huffman decoder in the style of zlib's "puff": small, simple, and fast enough for level data.
struct Huff {
    uint16_t count[16];
    uint16_t symbol[320];
};

// Returns 0 for a complete code, >0 for an incomplete one, <0 for an over-subscribed one.
int construct(Huff& h, const uint8_t* length, int n) {
    for (int l = 0; l <= 15; ++l) h.count[l] = 0;
    for (int s = 0; s < n; ++s) h.count[length[s]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int l = 1; l <= 15; ++l) {
        left <<= 1;
        left -= h.count[l];
        if (left < 0) return left;
    }
    uint16_t offs[16];
    offs[1] = 0;
    for (int l = 1; l < 15; ++l) offs[l + 1] = (uint16_t)(offs[l] + h.count[l]);
    for (int s = 0; s < n; ++s)
        if (length[s] != 0) h.symbol[offs[length[s]]++] = (uint16_t)s;
    return left;
}

struct Inflater {
    const uint8_t* in;
    size_t inSize, inPos = 0;
    uint32_t bitBuf = 0;
    int bitCnt = 0;
    std::vector<uint8_t>& out;
    size_t maxOut;
    bool err = false;

    Inflater(const uint8_t* d, size_t n, std::vector<uint8_t>& o, size_t m) : in(d), inSize(n), out(o), maxOut(m) {}

    int bits(int need) {
        uint32_t val = bitBuf;
        while (bitCnt < need) {
            if (inPos >= inSize) { err = true; return 0; }
            val |= (uint32_t)in[inPos++] << bitCnt;
            bitCnt += 8;
        }
        bitBuf = val >> need;
        bitCnt -= need;
        return (int)(val & ((1u << need) - 1));
    }

    int decode(const Huff& h) {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= 15; ++len) {
            code |= bits(1);
            if (err) return -1;
            int count = h.count[len];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        err = true;
        return -1;
    }

    bool codes(const Huff& lit, const Huff& dist) {
        static const uint16_t lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                           35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const uint8_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const uint16_t dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                           1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const uint8_t dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            int sym = decode(lit);
            if (err) return false;
            if (sym < 256) {
                if (out.size() >= maxOut) return false;
                out.push_back((uint8_t)sym);
            } else if (sym == 256) {
                return true;
            } else {
                sym -= 257;
                if (sym >= 29) return false;
                int len = lbase[sym] + bits(lext[sym]);
                int ds = decode(dist);
                if (err || ds < 0 || ds >= 30) return false;
                size_t d = (size_t)dbase[ds] + (size_t)bits(dext[ds]);
                if (err || d > out.size() || out.size() + (size_t)len > maxOut) return false;
                size_t from = out.size() - d;
                for (int i = 0; i < len; ++i) out.push_back(out[from + i]);  // may overlap: byte by byte
            }
        }
    }

    bool stored() {
        bitBuf = 0;
        bitCnt = 0;
        if (inPos + 4 > inSize) return false;
        unsigned len = in[inPos] | (in[inPos + 1] << 8);
        unsigned nlen = in[inPos + 2] | (in[inPos + 3] << 8);
        inPos += 4;
        if (len != (~nlen & 0xFFFF)) return false;
        if (inPos + len > inSize || out.size() + len > maxOut) return false;
        out.insert(out.end(), in + inPos, in + inPos + len);
        inPos += len;
        return true;
    }

    bool fixedBlock() {
        static Huff lit, dist;
        static bool built = false;
        if (!built) {
            uint8_t l[288];
            int s = 0;
            for (; s < 144; ++s) l[s] = 8;
            for (; s < 256; ++s) l[s] = 9;
            for (; s < 280; ++s) l[s] = 7;
            for (; s < 288; ++s) l[s] = 8;
            construct(lit, l, 288);
            for (s = 0; s < 30; ++s) l[s] = 5;
            construct(dist, l, 30);
            built = true;
        }
        return codes(lit, dist);
    }

    bool dynamicBlock() {
        static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        int nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
        if (err || nlen > 286 || ndist > 30) return false;
        uint8_t lengths[320];
        int i = 0;
        for (; i < ncode; ++i) lengths[order[i]] = (uint8_t)bits(3);
        for (; i < 19; ++i) lengths[order[i]] = 0;
        if (err) return false;
        Huff lencode;
        if (construct(lencode, lengths, 19) != 0) return false;
        i = 0;
        while (i < nlen + ndist) {
            int sym = decode(lencode);
            if (err) return false;
            if (sym < 16) {
                lengths[i++] = (uint8_t)sym;
            } else {
                int len = 0, rep;
                if (sym == 16) {
                    if (i == 0) return false;
                    len = lengths[i - 1];
                    rep = 3 + bits(2);
                } else if (sym == 17) {
                    rep = 3 + bits(3);
                } else {
                    rep = 11 + bits(7);
                }
                if (err || i + rep > nlen + ndist) return false;
                while (rep--) lengths[i++] = (uint8_t)len;
            }
        }
        if (lengths[256] == 0) return false;  // no end-of-block code
        Huff lit, dist;
        int e = construct(lit, lengths, nlen);
        if (e < 0 || (e > 0 && nlen - lit.count[0] != 1)) return false;
        e = construct(dist, lengths + nlen, ndist);
        if (e < 0 || (e > 0 && ndist - dist.count[0] != 1)) return false;
        return codes(lit, dist);
    }

    bool run() {
        int last;
        do {
            last = bits(1);
            int type = bits(2);
            if (err) return false;
            bool ok;
            if (type == 0) ok = stored();
            else if (type == 1) ok = fixedBlock();
            else if (type == 2) ok = dynamicBlock();
            else return false;
            if (!ok || err) return false;
        } while (!last);
        return true;
    }
};


bool inflateCore(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut, size_t* consumed) {
    Inflater inf(data, size, out, maxOut);
    bool ok = inf.run();
    if (consumed) *consumed = inf.inPos;
    return ok;
}

uint32_t adler32(const uint8_t* d, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) {
        a = (a + d[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

} // namespace

bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(in.size() * 3 / 4);
    uint32_t acc = 0;
    int nbits = 0;
    for (unsigned char c : in) {
        if (c == '=' || c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int v = b64val(c);
        if (v < 0) return false;
        acc = (acc << 6) | (uint32_t)v;
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            out.push_back((uint8_t)((acc >> nbits) & 0xFF));
        }
    }
    return true;
}

bool inflateRaw(const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t maxOut) {
    out.clear();
    return inflateCore(data, size, out, maxOut, nullptr);
}

uint32_t crc32(const uint8_t* data, size_t size) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool gunzip(const uint8_t* d, size_t n, std::vector<uint8_t>& out, size_t maxOut) {
    out.clear();
    if (n < 18 || d[0] != 0x1f || d[1] != 0x8b || d[2] != 8) return false;
    const uint8_t flg = d[3];
    size_t p = 10;
    if (flg & 4) {  // FEXTRA
        if (p + 2 > n) return false;
        p += 2 + (d[p] | (d[p + 1] << 8));
    }
    if (flg & 8) { while (p < n && d[p]) ++p; ++p; }   // FNAME
    if (flg & 16) { while (p < n && d[p]) ++p; ++p; }  // FCOMMENT
    if (flg & 2) p += 2;                               // FHCRC
    if (p + 8 > n) return false;
    size_t used = 0;
    if (!inflateCore(d + p, n - p - 8, out, maxOut, &used)) { out.clear(); return false; }
    const uint8_t* t = d + p + used;
    if (p + used + 8 > n) { out.clear(); return false; }
    uint32_t crc = t[0] | (t[1] << 8) | (t[2] << 16) | ((uint32_t)t[3] << 24);
    uint32_t isize = t[4] | (t[5] << 8) | (t[6] << 16) | ((uint32_t)t[7] << 24);
    if (crc != crc32(out.data(), out.size()) || isize != (uint32_t)out.size()) { out.clear(); return false; }
    return true;
}

bool zlibDecompress(const uint8_t* d, size_t n, std::vector<uint8_t>& out, size_t maxOut) {
    out.clear();
    if (n < 6 || (d[0] & 0x0F) != 8 || ((d[0] << 8) | d[1]) % 31 != 0 || (d[1] & 0x20)) return false;
    size_t used = 0;
    if (!inflateCore(d + 2, n - 2 - 4, out, maxOut, &used)) { out.clear(); return false; }
    if (2 + used + 4 > n) { out.clear(); return false; }
    const uint8_t* t = d + 2 + used;
    uint32_t ad = ((uint32_t)t[0] << 24) | (t[1] << 16) | (t[2] << 8) | t[3];
    if (ad != adler32(out.data(), out.size())) { out.clear(); return false; }
    return true;
}

bool decompressAuto(const uint8_t* d, size_t n, std::vector<uint8_t>& out, size_t maxOut) {
    if (n >= 2 && d[0] == 0x1f && d[1] == 0x8b) return gunzip(d, n, out, maxOut);
    if (n >= 2 && (d[0] & 0x0F) == 8 && ((d[0] << 8) | d[1]) % 31 == 0) return zlibDecompress(d, n, out, maxOut);
    return inflateRaw(d, n, out, maxOut);
}

} // namespace ogd
