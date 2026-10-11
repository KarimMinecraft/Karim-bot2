#pragma once
// ============================================================
// Penulis video MJPEG-AVI buatan sendiri (tanpa library luar).
//  - JpegEncoder : RGBA -> JPEG baseline (4:4:4)
//  - AviWriter   : kumpulan JPEG -> file .avi yang bisa diputar di VLC / MX Player, dll.
// Murni C++ standar, tidak bergantung pada Geode.
// ============================================================
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace kbvid {

class JpegEncoder {
public:
    // rgba: piksel sumber (srcW x srcH, 4 byte per piksel). Hasil diperkecil ke outW x outH.
    static std::vector<uint8_t> encode(uint8_t const* rgba, int srcW, int srcH, int outW, int outH, int quality) {
        JpegEncoder e;
        return e.run(rgba, srcW, srcH, outW, outH, quality);
    }

private:
    struct HuffTable {
        uint16_t code[256];
        uint8_t len[256];
        uint8_t bits[17];
        std::vector<uint8_t> vals;
    };

    std::vector<uint8_t> out;
    uint32_t bitBuf = 0;
    int bitCnt = 0;
    HuffTable dc, ac;
    float cosT[8][8];
    int qt[64];   // dalam urutan zigzag
    static constexpr int ZZ[64] = {
        0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
        12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
    };

    void buildTable(HuffTable& t, std::vector<std::pair<int, std::vector<int>>> const& groups) {
        std::memset(&t.code, 0, sizeof(t.code));
        std::memset(&t.len, 0, sizeof(t.len));
        std::memset(&t.bits, 0, sizeof(t.bits));
        t.vals.clear();
        // groups: (panjang kode, daftar simbol); harus terurut naik menurut panjang
        int code = 0, prevLen = 0;
        for (auto const& g : groups) {
            int L = g.first;
            code <<= (L - prevLen);
            prevLen = L;
            for (int sym : g.second) {
                t.code[sym] = static_cast<uint16_t>(code);
                t.len[sym] = static_cast<uint8_t>(L);
                t.bits[L]++;
                t.vals.push_back(static_cast<uint8_t>(sym));
                code++;
            }
        }
    }

    void initTables() {
        // DC: kategori 0..11, semua panjang 4 bit
        std::vector<int> dcs;
        for (int i = 0; i < 12; i++) dcs.push_back(i);
        buildTable(dc, {{4, dcs}});

        // AC: semua pasangan (run 0..15, ukuran 1..10) + EOB (0x00) + ZRL (0xF0)
        std::vector<int> A = {0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x21, 0x12};
        std::vector<int> B = {0x31, 0x05, 0x41, 0x51, 0x13, 0x22, 0x06, 0x61, 0x14, 0x32, 0x07, 0x71, 0x81, 0x23, 0x42, 0x15};
        std::vector<bool> used(256, false);
        for (int s : A) used[s] = true;
        for (int s : B) used[s] = true;
        std::vector<int> C;
        for (int r = 0; r < 16; r++) {
            for (int s = 1; s <= 10; s++) {
                int sym = (r << 4) | s;
                if (!used[sym]) { C.push_back(sym); used[sym] = true; }
            }
        }
        if (!used[0xF0]) { C.push_back(0xF0); used[0xF0] = true; }
        buildTable(ac, {{4, A}, {7, B}, {9, C}});
    }

    void initQuant(int quality) {
        static const int base[64] = {
            16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
            14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
            18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
            49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99
        };
        quality = (std::max)(1, (std::min)(100, quality));
        int scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
        for (int i = 0; i < 64; i++) {
            int v = (base[ZZ[i]] * scale + 50) / 100;
            qt[i] = (std::max)(1, (std::min)(255, v));
        }
    }

    void put8(int v) { out.push_back(static_cast<uint8_t>(v)); }
    void put16(int v) { put8(v >> 8); put8(v & 0xFF); }

    void writeBits(uint32_t code, int n) {
        bitBuf = (bitBuf << n) | (code & ((1u << n) - 1));
        bitCnt += n;
        while (bitCnt >= 8) {
            uint8_t b = static_cast<uint8_t>((bitBuf >> (bitCnt - 8)) & 0xFF);
            out.push_back(b);
            if (b == 0xFF) out.push_back(0x00);
            bitCnt -= 8;
        }
    }

    void flushBits() {
        if (bitCnt > 0) writeBits(0x7F, 8 - bitCnt);
    }

    static void category(int v, int& cat, uint32_t& bitsOut) {
        int a = v < 0 ? -v : v;
        cat = 0;
        while (a) { cat++; a >>= 1; }
        bitsOut = v >= 0 ? static_cast<uint32_t>(v) : static_cast<uint32_t>(v + (1 << cat) - 1);
    }

    void encodeBlock(float const* px, int& prevDC) {
        // DCT 2D terpisah: tmp = C * X, coef = tmp * C^T
        float tmp[64], coef[64];
        for (int u = 0; u < 8; u++) {
            for (int x = 0; x < 8; x++) {
                float s = 0.f;
                for (int y = 0; y < 8; y++) s += cosT[u][y] * px[y * 8 + x];
                tmp[u * 8 + x] = s;
            }
        }
        for (int u = 0; u < 8; u++) {
            for (int v = 0; v < 8; v++) {
                float s = 0.f;
                for (int x = 0; x < 8; x++) s += tmp[u * 8 + x] * cosT[v][x];
                coef[u * 8 + v] = s;
            }
        }
        int q[64];
        for (int i = 0; i < 64; i++) {
            float c = coef[ZZ[i]] / static_cast<float>(qt[i]);
            q[i] = static_cast<int>(std::lround(c));
        }

        // DC
        int diff = q[0] - prevDC;
        prevDC = q[0];
        int cat; uint32_t bitsV;
        category(diff, cat, bitsV);
        writeBits(dc.code[cat], dc.len[cat]);
        if (cat) writeBits(bitsV, cat);

        // AC
        int run = 0;
        for (int i = 1; i < 64; i++) {
            if (q[i] == 0) { run++; continue; }
            while (run > 15) {
                writeBits(ac.code[0xF0], ac.len[0xF0]);
                run -= 16;
            }
            category(q[i], cat, bitsV);
            if (cat > 10) cat = 10;
            int sym = (run << 4) | cat;
            writeBits(ac.code[sym], ac.len[sym]);
            writeBits(bitsV, cat);
            run = 0;
        }
        if (run > 0) writeBits(ac.code[0x00], ac.len[0x00]);
    }

    void writeDHT(int tcth, HuffTable const& t) {
        put16(0xFFC4);
        put16(2 + 1 + 16 + static_cast<int>(t.vals.size()));
        put8(tcth);
        for (int i = 1; i <= 16; i++) put8(t.bits[i]);
        for (uint8_t v : t.vals) put8(v);
    }

    std::vector<uint8_t> run(uint8_t const* rgba, int srcW, int srcH, int outW, int outH, int quality) {
        out.clear();
        bitBuf = 0;
        bitCnt = 0;
        initTables();
        initQuant(quality);
        for (int u = 0; u < 8; u++) {
            for (int x = 0; x < 8; x++) {
                float cu = (u == 0) ? std::sqrt(1.f / 8.f) : std::sqrt(2.f / 8.f);
                cosT[u][x] = cu * std::cos((2.f * x + 1.f) * u * 3.14159265358979f / 16.f);
            }
        }

        out.reserve(static_cast<size_t>(outW) * outH / 4);

        // SOI + JFIF
        put16(0xFFD8);
        put16(0xFFE0); put16(16);
        put8('J'); put8('F'); put8('I'); put8('F'); put8(0);
        put8(1); put8(1); put8(0); put16(1); put16(1); put8(0); put8(0);
        // DQT (satu tabel dipakai semua komponen)
        put16(0xFFDB); put16(67); put8(0);
        for (int i = 0; i < 64; i++) put8(qt[i]);
        // SOF0
        put16(0xFFC0); put16(17); put8(8); put16(outH); put16(outW); put8(3);
        put8(1); put8(0x11); put8(0);
        put8(2); put8(0x11); put8(0);
        put8(3); put8(0x11); put8(0);
        // DHT
        writeDHT(0x00, dc);
        writeDHT(0x10, ac);
        // SOS
        put16(0xFFDA); put16(12); put8(3);
        put8(1); put8(0x00);
        put8(2); put8(0x00);
        put8(3); put8(0x00);
        put8(0); put8(63); put8(0);

        // Peta koordinat keluaran -> sumber (nearest neighbour)
        std::vector<int> mapX(outW), mapY(outH);
        for (int x = 0; x < outW; x++) mapX[x] = (std::min)(srcW - 1, static_cast<int>((static_cast<int64_t>(x) * srcW) / outW));
        for (int y = 0; y < outH; y++) mapY[y] = (std::min)(srcH - 1, static_cast<int>((static_cast<int64_t>(y) * srcH) / outH));

        int blocksX = (outW + 7) / 8, blocksY = (outH + 7) / 8;
        int prevY = 0, prevCb = 0, prevCr = 0;
        float by[64], bcb[64], bcr[64];
        for (int byi = 0; byi < blocksY; byi++) {
            for (int bxi = 0; bxi < blocksX; bxi++) {
                for (int j = 0; j < 8; j++) {
                    int yy = (std::min)(outH - 1, byi * 8 + j);
                    uint8_t const* row = rgba + static_cast<size_t>(mapY[yy]) * srcW * 4;
                    for (int i = 0; i < 8; i++) {
                        int xx = (std::min)(outW - 1, bxi * 8 + i);
                        uint8_t const* p = row + static_cast<size_t>(mapX[xx]) * 4;
                        float r = p[0], g = p[1], b = p[2];
                        by[j * 8 + i] = 0.299f * r + 0.587f * g + 0.114f * b - 128.f;
                        bcb[j * 8 + i] = -0.168736f * r - 0.331264f * g + 0.5f * b;
                        bcr[j * 8 + i] = 0.5f * r - 0.418688f * g - 0.081312f * b;
                    }
                }
                encodeBlock(by, prevY);
                encodeBlock(bcb, prevCb);
                encodeBlock(bcr, prevCr);
            }
        }
        flushBits();
        put16(0xFFD9);
        return std::move(out);
    }
};

// ------------------------------------------------------------
// Penulis AVI (video MJPEG, tanpa audio)
// ------------------------------------------------------------
class AviWriter {
public:
    bool open(std::string const& path, int w, int h, int fps) {
        m_f.open(path, std::ios::binary | std::ios::trunc);
        if (!m_f) return false;
        m_w = w; m_h = h; m_fps = fps;
        m_frames = 0;
        m_maxFrame = 0;
        m_index.clear();

        // RIFF 'AVI '
        fourcc("RIFF"); u32(0); fourcc("AVI ");
        // LIST hdrl
        fourcc("LIST"); u32(4 + 8 + 56 + 12 + 8 + 56 + 8 + 40); fourcc("hdrl");
        fourcc("avih"); u32(56);
        m_posAvih = m_f.tellp();
        u32(1000000 / fps);          // dwMicroSecPerFrame
        u32(0);                      // dwMaxBytesPerSec
        u32(0);                      // dwPaddingGranularity
        u32(0x10);                   // dwFlags: AVIF_HASINDEX
        u32(0);                      // dwTotalFrames (diisi saat close)
        u32(0);                      // dwInitialFrames
        u32(1);                      // dwStreams
        u32(0);                      // dwSuggestedBufferSize
        u32(w); u32(h);
        u32(0); u32(0); u32(0); u32(0);
        fourcc("LIST"); u32(4 + 8 + 56 + 8 + 40); fourcc("strl");
        fourcc("strh"); u32(56);
        m_posStrh = m_f.tellp();
        fourcc("vids"); fourcc("MJPG");
        u32(0); u16(0); u16(0);      // flags, priority, language
        u32(0);                      // initial frames
        u32(1); u32(fps);            // scale, rate
        u32(0);                      // start
        u32(0);                      // length (diisi saat close)
        u32(0);                      // suggested buffer
        u32(0xFFFFFFFFu);            // quality
        u32(0);                      // sample size
        u16(0); u16(0); u16(static_cast<uint16_t>(w)); u16(static_cast<uint16_t>(h));
        fourcc("strf"); u32(40);
        u32(40); u32(w); u32(h); u16(1); u16(24); fourcc("MJPG");
        u32(static_cast<uint32_t>(w) * h * 3);
        u32(0); u32(0); u32(0); u32(0);
        // LIST movi
        fourcc("LIST");
        m_posMoviSize = m_f.tellp();
        u32(0);
        fourcc("movi");
        m_moviStart = static_cast<uint32_t>(m_f.tellp()) - 4;   // posisi tulisan "movi"
        return static_cast<bool>(m_f);
    }

    bool isOpen() const { return m_f.is_open(); }
    int frames() const { return m_frames; }

    bool addFrame(std::vector<uint8_t> const& jpeg) {
        if (!m_f) return false;
        uint32_t off = static_cast<uint32_t>(m_f.tellp()) - m_moviStart;
        fourcc("00dc");
        u32(static_cast<uint32_t>(jpeg.size()));
        m_f.write(reinterpret_cast<char const*>(jpeg.data()), static_cast<std::streamsize>(jpeg.size()));
        if (jpeg.size() & 1) m_f.put('\0');
        m_index.push_back({off, static_cast<uint32_t>(jpeg.size())});
        m_maxFrame = (std::max<uint32_t>)(m_maxFrame, static_cast<uint32_t>(jpeg.size()));
        m_frames++;
        return static_cast<bool>(m_f);
    }

    bool close() {
        if (!m_f.is_open()) return false;
        uint32_t moviEnd = static_cast<uint32_t>(m_f.tellp());
        // idx1
        fourcc("idx1");
        u32(static_cast<uint32_t>(m_index.size()) * 16);
        for (auto const& e : m_index) {
            fourcc("00dc"); u32(0x10); u32(e.first); u32(e.second);
        }
        uint32_t fileEnd = static_cast<uint32_t>(m_f.tellp());

        // tambal ukuran
        patch(4, fileEnd - 8);
        patch(static_cast<uint32_t>(m_posMoviSize), moviEnd - static_cast<uint32_t>(m_posMoviSize) - 4);
        patch(static_cast<uint32_t>(m_posAvih) + 4, static_cast<uint32_t>(m_maxFrame) * static_cast<uint32_t>(m_fps));
        patch(static_cast<uint32_t>(m_posAvih) + 16, static_cast<uint32_t>(m_frames));
        patch(static_cast<uint32_t>(m_posAvih) + 28, m_maxFrame);
        patch(static_cast<uint32_t>(m_posStrh) + 32, static_cast<uint32_t>(m_frames));
        patch(static_cast<uint32_t>(m_posStrh) + 36, m_maxFrame);
        m_f.close();
        return true;
    }

private:
    std::ofstream m_f;
    int m_w = 0, m_h = 0, m_fps = 30, m_frames = 0;
    uint32_t m_maxFrame = 0;
    std::streampos m_posAvih = 0, m_posStrh = 0, m_posMoviSize = 0;
    uint32_t m_moviStart = 0;
    std::vector<std::pair<uint32_t, uint32_t>> m_index;

    void fourcc(char const* s) { m_f.write(s, 4); }
    void u32(uint32_t v) {
        char b[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                     static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
        m_f.write(b, 4);
    }
    void u16(uint16_t v) {
        char b[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
        m_f.write(b, 2);
    }
    void patch(uint32_t pos, uint32_t v) {
        m_f.seekp(pos);
        u32(v);
    }
};

} // namespace kbvid
