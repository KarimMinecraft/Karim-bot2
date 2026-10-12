#pragma once
// ============================================================
// Menambah suara (musik level) ke video MP4 setelah render selesai.
// Musik di-decode, dipotong sesuai tiap percobaan (attempt), di-encode AAC,
// lalu digabung dengan video tanpa menyentuh video aslinya (remux).
// Kalau gagal, video tanpa suara tetap utuh.
// Memakai MediaCodec/MediaExtractor Android lewat dlopen.
// ============================================================
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "Mp4Enc.hpp"

namespace kbvid {

struct AudioSeg {
    double videoSec;   // detik di video
    double songSec;    // posisi lagu pada saat itu
};

#ifdef GEODE_IS_ANDROID

struct AuApi {
    bool ok = false;
    void* (*exNew)() = nullptr;
    int32_t (*exDelete)(void*) = nullptr;
    int32_t (*exSetSrc)(void*, char const*) = nullptr;
    size_t (*exTracks)(void*) = nullptr;
    void* (*exFmt)(void*, size_t) = nullptr;
    int32_t (*exSelect)(void*, size_t) = nullptr;
    ssize_t (*exRead)(void*, uint8_t*, size_t) = nullptr;
    uint32_t (*exFlags)(void*) = nullptr;
    int64_t (*exTime)(void*) = nullptr;
    bool (*exAdvance)(void*) = nullptr;
    bool (*fmtGetStr)(void*, char const*, char const**) = nullptr;
    bool (*fmtGetI32)(void*, char const*, int32_t*) = nullptr;
    void (*fmtSetStr)(void*, char const*, char const*) = nullptr;
    void (*fmtSetI32)(void*, char const*, int32_t) = nullptr;
    void* (*fmtNew)() = nullptr;
    int32_t (*fmtDelete)(void*) = nullptr;
    void* (*decCreate)(char const*) = nullptr;
    void* (*encCreate)(char const*) = nullptr;
    int32_t (*configure)(void*, void const*, void*, void*, uint32_t) = nullptr;
    int32_t (*cStart)(void*) = nullptr;
    int32_t (*cStop)(void*) = nullptr;
    int32_t (*cDelete)(void*) = nullptr;
    ssize_t (*deqIn)(void*, int64_t) = nullptr;
    uint8_t* (*inBuf)(void*, size_t, size_t*) = nullptr;
    int32_t (*queueIn)(void*, size_t, off_t, size_t, uint64_t, uint32_t) = nullptr;
    ssize_t (*deqOut)(void*, McInfo*, int64_t) = nullptr;
    uint8_t* (*outBuf)(void*, size_t, size_t*) = nullptr;
    void* (*outFmt)(void*) = nullptr;
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

    static AuApi& get() {
        static AuApi a = load();
        return a;
    }

    static AuApi load() {
        AuApi a;
        void* h = dlopen("libmediandk.so", RTLD_NOW);
        if (!h) return a;
        bool ok = true;
        ok &= sym(h, "AMediaExtractor_new", a.exNew);
        ok &= sym(h, "AMediaExtractor_delete", a.exDelete);
        ok &= sym(h, "AMediaExtractor_setDataSource", a.exSetSrc);
        ok &= sym(h, "AMediaExtractor_getTrackCount", a.exTracks);
        ok &= sym(h, "AMediaExtractor_getTrackFormat", a.exFmt);
        ok &= sym(h, "AMediaExtractor_selectTrack", a.exSelect);
        ok &= sym(h, "AMediaExtractor_readSampleData", a.exRead);
        ok &= sym(h, "AMediaExtractor_getSampleFlags", a.exFlags);
        ok &= sym(h, "AMediaExtractor_getSampleTime", a.exTime);
        ok &= sym(h, "AMediaExtractor_advance", a.exAdvance);
        ok &= sym(h, "AMediaFormat_getString", a.fmtGetStr);
        ok &= sym(h, "AMediaFormat_getInt32", a.fmtGetI32);
        ok &= sym(h, "AMediaFormat_setString", a.fmtSetStr);
        ok &= sym(h, "AMediaFormat_setInt32", a.fmtSetI32);
        ok &= sym(h, "AMediaFormat_new", a.fmtNew);
        ok &= sym(h, "AMediaFormat_delete", a.fmtDelete);
        ok &= sym(h, "AMediaCodec_createDecoderByType", a.decCreate);
        ok &= sym(h, "AMediaCodec_createEncoderByType", a.encCreate);
        ok &= sym(h, "AMediaCodec_configure", a.configure);
        ok &= sym(h, "AMediaCodec_start", a.cStart);
        ok &= sym(h, "AMediaCodec_stop", a.cStop);
        ok &= sym(h, "AMediaCodec_delete", a.cDelete);
        ok &= sym(h, "AMediaCodec_dequeueInputBuffer", a.deqIn);
        ok &= sym(h, "AMediaCodec_getInputBuffer", a.inBuf);
        ok &= sym(h, "AMediaCodec_queueInputBuffer", a.queueIn);
        ok &= sym(h, "AMediaCodec_dequeueOutputBuffer", a.deqOut);
        ok &= sym(h, "AMediaCodec_getOutputBuffer", a.outBuf);
        ok &= sym(h, "AMediaCodec_getOutputFormat", a.outFmt);
        ok &= sym(h, "AMediaCodec_releaseOutputBuffer", a.relOut);
        ok &= sym(h, "AMediaMuxer_new", a.muxNew);
        ok &= sym(h, "AMediaMuxer_addTrack", a.muxAddTrack);
        ok &= sym(h, "AMediaMuxer_start", a.muxStart);
        ok &= sym(h, "AMediaMuxer_stop", a.muxStop);
        ok &= sym(h, "AMediaMuxer_delete", a.muxDelete);
        ok &= sym(h, "AMediaMuxer_writeSampleData", a.muxWrite);
        a.ok = ok;
        return a;
    }
};

struct AacChunk {
    int64_t pts;
    uint32_t flags;
    std::vector<uint8_t> data;
};

// Decode seluruh lagu jadi PCM 16-bit (maksimal maxSec detik)
inline bool decodeSong(AuApi& a, std::string const& file, double maxSec,
                       std::vector<int16_t>& pcm, int& rate, int& ch) {
    void* ex = a.exNew();
    if (!ex) return false;
    if (a.exSetSrc(ex, file.c_str()) != 0) {
        a.exDelete(ex);
        return false;
    }
    size_t n = a.exTracks(ex);
    int track = -1;
    void* fmt = nullptr;
    std::string mime;
    for (size_t i = 0; i < n; i++) {
        void* f = a.exFmt(ex, i);
        char const* m = nullptr;
        if (f && a.fmtGetStr(f, "mime", &m) && m && std::strncmp(m, "audio/", 6) == 0) {
            track = static_cast<int>(i);
            fmt = f;
            mime = m;
            break;
        }
        if (f) a.fmtDelete(f);
    }
    if (track < 0) {
        a.exDelete(ex);
        return false;
    }
    a.exSelect(ex, static_cast<size_t>(track));

    rate = 44100;
    ch = 2;
    int32_t v = 0;
    if (a.fmtGetI32(fmt, "sample-rate", &v) && v > 0) rate = v;
    if (a.fmtGetI32(fmt, "channel-count", &v) && v > 0) ch = v;

    void* dec = a.decCreate(mime.c_str());
    if (!dec) {
        a.fmtDelete(fmt);
        a.exDelete(ex);
        return false;
    }
    int32_t st = a.configure(dec, fmt, nullptr, nullptr, 0);
    a.fmtDelete(fmt);
    if (st != 0 || a.cStart(dec) != 0) {
        a.cDelete(dec);
        a.exDelete(ex);
        return false;
    }

    size_t maxSamples = static_cast<size_t>(maxSec * rate) * 2 + 4096;
    std::vector<uint8_t> tmp;
    bool inDone = false, outDone = false;
    for (int guard = 0; guard < 4000000 && !outDone; guard++) {
        if (!inDone) {
            ssize_t i = a.deqIn(dec, 2000);
            if (i >= 0) {
                size_t cap = 0;
                uint8_t* b = a.inBuf(dec, static_cast<size_t>(i), &cap);
                ssize_t sz = b ? a.exRead(ex, b, cap) : -1;
                if (sz < 0) {
                    a.queueIn(dec, static_cast<size_t>(i), 0, 0, 0, 4);
                    inDone = true;
                } else {
                    int64_t t = a.exTime(ex);
                    a.queueIn(dec, static_cast<size_t>(i), 0, static_cast<size_t>(sz), static_cast<uint64_t>(t < 0 ? 0 : t), 0);
                    a.exAdvance(ex);
                }
            }
        }
        McInfo info{};
        ssize_t o = a.deqOut(dec, &info, 2000);
        if (o == -2) {
            void* of = a.outFmt(dec);
            if (of) {
                int32_t q = 0;
                if (a.fmtGetI32(of, "sample-rate", &q) && q > 0) rate = q;
                if (a.fmtGetI32(of, "channel-count", &q) && q > 0) ch = q;
                a.fmtDelete(of);
            }
        } else if (o >= 0) {
            size_t sz = 0;
            uint8_t* b = a.outBuf(dec, static_cast<size_t>(o), &sz);
            if (b && info.size > 0) {
                auto const* s = reinterpret_cast<int16_t const*>(b + info.offset);
                size_t cnt = static_cast<size_t>(info.size) / 2;
                pcm.insert(pcm.end(), s, s + cnt);
            }
            a.relOut(dec, static_cast<size_t>(o), false);
            if (info.flags & 4) outDone = true;
            if (pcm.size() > maxSamples) outDone = true;
        }
    }
    a.cStop(dec);
    a.cDelete(dec);
    a.exDelete(ex);
    if (ch < 1) ch = 1;
    return !pcm.empty();
}

// Gabungkan lagu per segmen -> encode AAC. Hasil: potongan AAC + format output (harus di-delete pemanggil)
inline bool encodeAac(AuApi& a, std::vector<int16_t> const& pcm, int rate, int ch,
                      std::vector<AudioSeg> const& segs, double totalSec,
                      std::vector<AacChunk>& out, void*& outFormat) {
    void* enc = a.encCreate("audio/mp4a-latm");
    if (!enc) return false;
    void* f = a.fmtNew();
    a.fmtSetStr(f, "mime", "audio/mp4a-latm");
    a.fmtSetI32(f, "sample-rate", rate);
    a.fmtSetI32(f, "channel-count", 2);
    a.fmtSetI32(f, "bitrate", 128000);
    a.fmtSetI32(f, "aac-profile", 2);
    int32_t st = a.configure(enc, f, nullptr, nullptr, 1);
    a.fmtDelete(f);
    if (st != 0 || a.cStart(enc) != 0) {
        a.cDelete(enc);
        return false;
    }

    int64_t totalFrames = static_cast<int64_t>(totalSec * rate);
    int64_t pos = 0;
    size_t segIdx = 0;
    int64_t songFrames = static_cast<int64_t>(pcm.size()) / ch;
    bool inDone = false, outDone = false;
    outFormat = nullptr;

    for (int guard = 0; guard < 4000000 && !outDone; guard++) {
        if (!inDone) {
            ssize_t i = a.deqIn(enc, 2000);
            if (i >= 0) {
                size_t cap = 0;
                uint8_t* b = a.inBuf(enc, static_cast<size_t>(i), &cap);
                int64_t nf = (std::min)(static_cast<int64_t>(1024), static_cast<int64_t>(cap / 4));
                nf = (std::min)(nf, totalFrames - pos);
                uint64_t pts = static_cast<uint64_t>(pos) * 1000000ull / static_cast<uint64_t>(rate);
                if (!b || nf <= 0) {
                    a.queueIn(enc, static_cast<size_t>(i), 0, 0, pts, 4);
                    inDone = true;
                } else {
                    auto* dst = reinterpret_cast<int16_t*>(b);
                    for (int64_t k = 0; k < nf; k++) {
                        double t = static_cast<double>(pos + k) / rate;
                        while (segIdx + 1 < segs.size() && segs[segIdx + 1].videoSec <= t) segIdx++;
                        int16_t l = 0, r = 0;
                        if (!segs.empty()) {
                            double songT = segs[segIdx].songSec + (t - segs[segIdx].videoSec);
                            int64_t sf = static_cast<int64_t>(std::llround(songT * rate));
                            if (sf >= 0 && sf < songFrames) {
                                l = pcm[static_cast<size_t>(sf) * ch];
                                r = pcm[static_cast<size_t>(sf) * ch + (ch > 1 ? 1 : 0)];
                            }
                        }
                        dst[k * 2] = l;
                        dst[k * 2 + 1] = r;
                    }
                    a.queueIn(enc, static_cast<size_t>(i), 0, static_cast<size_t>(nf) * 4, pts, 0);
                    pos += nf;
                }
            }
        }
        McInfo info{};
        ssize_t o = a.deqOut(enc, &info, 2000);
        if (o == -2) {
            if (outFormat) a.fmtDelete(outFormat);
            outFormat = a.outFmt(enc);
        } else if (o >= 0) {
            size_t sz = 0;
            uint8_t* b = a.outBuf(enc, static_cast<size_t>(o), &sz);
            if (b && info.size > 0 && !(info.flags & 2)) {
                AacChunk c;
                c.pts = info.pts;
                c.flags = 0;
                c.data.assign(b + info.offset, b + info.offset + info.size);
                out.push_back(std::move(c));
            }
            a.relOut(enc, static_cast<size_t>(o), false);
            if (info.flags & 4) outDone = true;
        }
    }
    a.cStop(enc);
    a.cDelete(enc);
    return outDone && outFormat && !out.empty();
}

inline bool addAudioImpl(std::string const& videoPath, std::string const& songFile,
                         std::vector<AudioSeg> const& segs, double totalSec) {
    auto& a = AuApi::get();
    if (!a.ok || segs.empty() || totalSec <= 0.0) return false;

    double maxSong = 0.0;
    for (auto const& s : segs) maxSong = (std::max)(maxSong, s.songSec);
    std::vector<int16_t> pcm;
    int rate = 44100, ch = 2;
    if (!decodeSong(a, songFile, maxSong + totalSec + 5.0, pcm, rate, ch)) return false;

    std::vector<AacChunk> chunks;
    void* aFmt = nullptr;
    if (!encodeAac(a, pcm, rate, ch, segs, totalSec, chunks, aFmt)) {
        if (aFmt) a.fmtDelete(aFmt);
        return false;
    }
    pcm.clear();
    pcm.shrink_to_fit();

    // --- gabung video + audio ---
    std::string tmpPath = videoPath + ".tmp";
    void* vex = a.exNew();
    if (!vex || a.exSetSrc(vex, videoPath.c_str()) != 0) {
        if (vex) a.exDelete(vex);
        a.fmtDelete(aFmt);
        return false;
    }
    size_t n = a.exTracks(vex);
    int vtrack = -1;
    void* vfmt = nullptr;
    for (size_t i = 0; i < n; i++) {
        void* f = a.exFmt(vex, i);
        char const* m = nullptr;
        if (f && a.fmtGetStr(f, "mime", &m) && m && std::strncmp(m, "video/", 6) == 0) {
            vtrack = static_cast<int>(i);
            vfmt = f;
            break;
        }
        if (f) a.fmtDelete(f);
    }
    if (vtrack < 0) {
        a.exDelete(vex);
        a.fmtDelete(aFmt);
        return false;
    }
    a.exSelect(vex, static_cast<size_t>(vtrack));

    int fd = ::open(tmpPath.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
    void* mux = fd >= 0 ? a.muxNew(fd, 0) : nullptr;
    if (!mux) {
        if (fd >= 0) ::close(fd);
        a.fmtDelete(vfmt);
        a.fmtDelete(aFmt);
        a.exDelete(vex);
        return false;
    }
    ssize_t vt = a.muxAddTrack(mux, vfmt);
    ssize_t at = a.muxAddTrack(mux, aFmt);
    a.fmtDelete(vfmt);
    a.fmtDelete(aFmt);
    bool ok = (vt >= 0 && at >= 0 && a.muxStart(mux) == 0);

    if (ok) {
        std::vector<uint8_t> vbuf(8 * 1024 * 1024);
        size_t ai = 0;
        bool vHave = false;
        McInfo vinfo{};
        auto readVideo = [&]() {
            ssize_t sz = a.exRead(vex, vbuf.data(), vbuf.size());
            if (sz < 0) {
                vHave = false;
                return;
            }
            vinfo.offset = 0;
            vinfo.size = static_cast<int32_t>(sz);
            vinfo.pts = a.exTime(vex);
            vinfo.flags = (a.exFlags(vex) & 1) ? 1u : 0u;
            vHave = true;
            a.exAdvance(vex);
        };
        readVideo();
        while (vHave || ai < chunks.size()) {
            bool takeVideo = vHave && (ai >= chunks.size() || vinfo.pts <= chunks[ai].pts);
            if (takeVideo) {
                a.muxWrite(mux, static_cast<size_t>(vt), vbuf.data(), &vinfo);
                readVideo();
            } else {
                McInfo ainfo{};
                ainfo.offset = 0;
                ainfo.size = static_cast<int32_t>(chunks[ai].data.size());
                ainfo.pts = chunks[ai].pts;
                ainfo.flags = 0;
                a.muxWrite(mux, static_cast<size_t>(at), chunks[ai].data.data(), &ainfo);
                ai++;
            }
        }
        a.muxStop(mux);
    }
    a.muxDelete(mux);
    ::close(fd);
    a.exDelete(vex);

    std::error_code ec;
    if (!ok) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    std::filesystem::rename(tmpPath, videoPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

inline bool addAudio(std::string const& videoPath, std::string const& songFile,
                     std::vector<AudioSeg> const& segs, double totalSec) {
    return addAudioImpl(videoPath, songFile, segs, totalSec);
}

#else

inline bool addAudio(std::string const&, std::string const&, std::vector<AudioSeg> const&, double) {
    return false;
}

#endif

} // namespace kbvid
