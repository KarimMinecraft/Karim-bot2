#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#ifdef GEODE_IS_ANDROID
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// ============================================================
// Penulis video MP4 (H.264) memakai encoder bawaan Android (libmediandk).
// Dimuat lewat dlopen, jadi tidak perlu pengaturan CMake tambahan.
// Di platform selain Android, open() selalu gagal (dipakai AVI sebagai cadangan).
// ============================================================
namespace kbvid {

#ifdef GEODE_IS_ANDROID
struct McInfo {
    int32_t offset;
    int32_t size;
    int64_t pts;
    uint32_t flags;
};

struct McApi {
    bool ok = false;
    void* (*fmtNew)() = nullptr;
    int32_t (*fmtDelete)(void*) = nullptr;
    void (*fmtSetString)(void*, char const*, char const*) = nullptr;
    void (*fmtSetInt32)(void*, char const*, int32_t) = nullptr;
    bool (*fmtGetInt32)(void*, char const*, int32_t*) = nullptr;
    void* (*encCreate)(char const*) = nullptr;
    int32_t (*encConfigure)(void*, void const*, void*, void*, uint32_t) = nullptr;
    int32_t (*encStart)(void*) = nullptr;
    int32_t (*encStop)(void*) = nullptr;
    int32_t (*encDelete)(void*) = nullptr;
    uint8_t* (*inBuf)(void*, size_t, size_t*) = nullptr;
    uint8_t* (*outBuf)(void*, size_t, size_t*) = nullptr;
    ssize_t (*deqIn)(void*, int64_t) = nullptr;
    int32_t (*queueIn)(void*, size_t, off_t, size_t, uint64_t, uint32_t) = nullptr;
    ssize_t (*deqOut)(void*, McInfo*, int64_t) = nullptr;
    void* (*outFmt)(void*) = nullptr;
    void* (*inFmt)(void*) = nullptr;   // boleh kosong (Android < 9)
    int32_t (*relOut)(void*, size_t, bool) = nullptr;
    void* (*muxNew)(int, int) = nullptr;
    ssize_t (*muxAddTrack)(void*, void const*) = nullptr;
    int32_t (*muxStart)(void*) = nullptr;
    int32_t (*muxStop)(void*) = nullptr;
    int32_t (*muxDelete)(void*) = nullptr;
    int32_t (*muxWrite)(void*, size_t, uint8_t const*, McInfo const*) = nullptr;

    template <class T>
    static bool sym(void* h, char const* name, T& out) {
        out = reinterpret_cast<T>(dlsym(h, name));
        return out != nullptr;
    }

    static McApi& get() {
        static McApi a = load();
        return a;
    }

    static McApi load() {
        McApi a;
        void* h = dlopen("libmediandk.so", RTLD_NOW);
        if (!h) return a;
        bool ok = true;
        ok &= sym(h, "AMediaFormat_new", a.fmtNew);
        ok &= sym(h, "AMediaFormat_delete", a.fmtDelete);
        ok &= sym(h, "AMediaFormat_setString", a.fmtSetString);
        ok &= sym(h, "AMediaFormat_setInt32", a.fmtSetInt32);
        ok &= sym(h, "AMediaFormat_getInt32", a.fmtGetInt32);
        ok &= sym(h, "AMediaCodec_createEncoderByType", a.encCreate);
        ok &= sym(h, "AMediaCodec_configure", a.encConfigure);
        ok &= sym(h, "AMediaCodec_start", a.encStart);
        ok &= sym(h, "AMediaCodec_stop", a.encStop);
        ok &= sym(h, "AMediaCodec_delete", a.encDelete);
        ok &= sym(h, "AMediaCodec_getInputBuffer", a.inBuf);
        ok &= sym(h, "AMediaCodec_getOutputBuffer", a.outBuf);
        ok &= sym(h, "AMediaCodec_dequeueInputBuffer", a.deqIn);
        ok &= sym(h, "AMediaCodec_queueInputBuffer", a.queueIn);
        ok &= sym(h, "AMediaCodec_dequeueOutputBuffer", a.deqOut);
        ok &= sym(h, "AMediaCodec_getOutputFormat", a.outFmt);
        ok &= sym(h, "AMediaCodec_releaseOutputBuffer", a.relOut);
        ok &= sym(h, "AMediaMuxer_new", a.muxNew);
        ok &= sym(h, "AMediaMuxer_addTrack", a.muxAddTrack);
        ok &= sym(h, "AMediaMuxer_start", a.muxStart);
        ok &= sym(h, "AMediaMuxer_stop", a.muxStop);
        ok &= sym(h, "AMediaMuxer_delete", a.muxDelete);
        ok &= sym(h, "AMediaMuxer_writeSampleData", a.muxWrite);
        sym(h, "AMediaCodec_getInputFormat", a.inFmt);
        a.ok = ok;
        return a;
    }
};
#endif

class Mp4Writer {
public:
    bool isOpen() const { return opened; }
    int frames() const { return frameCount; }

    bool open(std::string const& path, int w, int h, int fps, int bitrate) {
#ifdef GEODE_IS_ANDROID
        auto& f = McApi::get();
        if (!f.ok || w < 16 || h < 16) return false;
        outW = w;
        outH = h;
        fpsV = fps < 1 ? 30 : fps;
        frameCount = 0;
        muxStarted = false;
        track = -1;
        stride = w;
        sliceH = h;

        codec = f.encCreate("video/avc");
        if (!codec) return false;

        void* fmt = f.fmtNew();
        f.fmtSetString(fmt, "mime", "video/avc");
        f.fmtSetInt32(fmt, "width", w);
        f.fmtSetInt32(fmt, "height", h);
        f.fmtSetInt32(fmt, "bitrate", bitrate);
        f.fmtSetInt32(fmt, "frame-rate", fpsV);
        f.fmtSetInt32(fmt, "i-frame-interval", 1);
        f.fmtSetInt32(fmt, "color-format", 21);   // NV12 (YUV420 semi-planar)
        int32_t st = f.encConfigure(codec, fmt, nullptr, nullptr, 1);
        f.fmtDelete(fmt);
        if (st != 0 || f.encStart(codec) != 0) {
            f.encDelete(codec);
            codec = nullptr;
            return false;
        }
        if (f.inFmt) {
            void* inf = f.inFmt(codec);
            if (inf) {
                int32_t v = 0;
                if (f.fmtGetInt32(inf, "stride", &v) && v >= w) stride = v;
                v = 0;
                if (f.fmtGetInt32(inf, "slice-height", &v) && v >= h) sliceH = v;
                f.fmtDelete(inf);
            }
        }

        fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            f.encStop(codec);
            f.encDelete(codec);
            codec = nullptr;
            return false;
        }
        muxer = f.muxNew(fd, 0);   // 0 = MPEG_4
        if (!muxer) {
            ::close(fd);
            fd = -1;
            f.encStop(codec);
            f.encDelete(codec);
            codec = nullptr;
            return false;
        }
        opened = true;
        return true;
#else
        (void)path; (void)w; (void)h; (void)fps; (void)bitrate;
        return false;
#endif
    }

    // rgba: gambar dari layar (kiri-atas = awal). Otomatis diperkecil ke ukuran video.
    bool addFrame(uint8_t const* rgba, int srcW, int srcH) {
#ifdef GEODE_IS_ANDROID
        if (!opened || !rgba || srcW < 1 || srcH < 1) return false;
        auto& f = McApi::get();

        ssize_t in = -1;
        for (int t = 0; t < 300 && in < 0; t++) {
            in = f.deqIn(codec, 10000);
            if (in < 0) drain(0);
        }
        if (in < 0) return false;

        size_t cap = 0;
        uint8_t* buf = f.inBuf(codec, static_cast<size_t>(in), &cap);
        size_t need = static_cast<size_t>(stride) * sliceH * 3 / 2;
        if (!buf || cap < need) {
            f.queueIn(codec, static_cast<size_t>(in), 0, 0, 0, 0);
            return false;
        }

        if (xmap.size() != static_cast<size_t>(outW) || mapSrcW != srcW) {
            xmap.resize(outW);
            for (int x = 0; x < outW; x++) xmap[x] = (x * srcW / outW) * 4;
            mapSrcW = srcW;
        }
        uint8_t* yp = buf;
        uint8_t* uvp = buf + static_cast<size_t>(stride) * sliceH;
        for (int y = 0; y < outH; y++) {
            int sy = y * srcH / outH;
            uint8_t const* row = rgba + static_cast<size_t>(sy) * srcW * 4;
            uint8_t* yo = yp + static_cast<size_t>(y) * stride;
            bool chroma = (y & 1) == 0;
            uint8_t* uvo = uvp + static_cast<size_t>(y / 2) * stride;
            for (int x = 0; x < outW; x++) {
                uint8_t const* p = row + xmap[x];
                int r = p[0], g = p[1], b = p[2];
                yo[x] = static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
                if (chroma && (x & 1) == 0) {
                    uvo[x] = static_cast<uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                    uvo[x + 1] = static_cast<uint8_t>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
                }
            }
        }

        uint64_t pts = static_cast<uint64_t>(frameCount) * 1000000ull / static_cast<uint64_t>(fpsV);
        if (f.queueIn(codec, static_cast<size_t>(in), 0, need, pts, 0) != 0) return false;
        frameCount++;
        drain(0);
        return true;
#else
        (void)rgba; (void)srcW; (void)srcH;
        return false;
#endif
    }

    void close() {
#ifdef GEODE_IS_ANDROID
        if (!opened) return;
        auto& f = McApi::get();
        ssize_t in = f.deqIn(codec, 200000);
        if (in >= 0) {
            uint64_t pts = static_cast<uint64_t>(frameCount) * 1000000ull / static_cast<uint64_t>(fpsV);
            f.queueIn(codec, static_cast<size_t>(in), 0, 0, pts, 4);   // 4 = akhir stream
            for (int i = 0; i < 100 && !eos; i++) drain(100000);
        }
        if (muxStarted) f.muxStop(muxer);
        f.muxDelete(muxer);
        muxer = nullptr;
        if (fd >= 0) ::close(fd);
        fd = -1;
        f.encStop(codec);
        f.encDelete(codec);
        codec = nullptr;
        opened = false;
        muxStarted = false;
        eos = false;
#endif
    }

private:
#ifdef GEODE_IS_ANDROID
    void drain(int64_t timeoutUs) {
        auto& f = McApi::get();
        McInfo info{};
        while (true) {
            ssize_t idx = f.deqOut(codec, &info, timeoutUs);
            if (idx == -2) {
                void* of = f.outFmt(codec);
                if (of && !muxStarted) {
                    track = static_cast<int>(f.muxAddTrack(muxer, of));
                    f.muxStart(muxer);
                    muxStarted = true;
                }
                if (of) f.fmtDelete(of);
                continue;
            }
            if (idx == -3) continue;
            if (idx < 0) break;
            size_t sz = 0;
            uint8_t* ob = f.outBuf(codec, static_cast<size_t>(idx), &sz);
            if (ob && info.size > 0 && !(info.flags & 2) && muxStarted && track >= 0) {
                f.muxWrite(muxer, static_cast<size_t>(track), ob, &info);
            }
            f.relOut(codec, static_cast<size_t>(idx), false);
            if (info.flags & 4) {
                eos = true;
                break;
            }
        }
    }

    void* codec = nullptr;
    void* muxer = nullptr;
    int fd = -1;
    int track = -1;
    bool muxStarted = false;
    bool eos = false;
    std::vector<int> xmap;
    int mapSrcW = 0;
#endif
    bool opened = false;
    int outW = 0, outH = 0, fpsV = 30;
    int stride = 0, sliceH = 0;
    int frameCount = 0;
};

} // namespace kbvid
