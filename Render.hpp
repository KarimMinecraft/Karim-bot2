#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>
#include "Mjpeg.hpp"
#include "Mp4Enc.hpp"
#include "Audio.hpp"

using namespace geode::prelude;

// ============================================================
// Pengelola rendering: merekam gameplay jadi video .avi (MJPEG)
//  - waktu game dikunci per frame (1 / FPS) -> video selalu mulus
//  - tiap frame: gambar scene ke texture -> encode JPEG -> tulis ke AVI
// ============================================================
class RenderManager {
public:
    static RenderManager& get() {
        static RenderManager instance;
        return instance;
    }

    // ---------- pengaturan (disimpan permanen) ----------
    static int clampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static int fps()        { return clampI(Mod::get()->getSavedValue<int>("r-fps", 30), 10, 60); }
    static void setFps(int v)        { Mod::get()->setSavedValue<int>("r-fps", clampI(v, 10, 60)); }
    static int heightP()    { return clampI(Mod::get()->getSavedValue<int>("r-height", 480), 240, 2160); }
    static void setHeightP(int v)    { Mod::get()->setSavedValue<int>("r-height", clampI(v, 240, 2160)); }
    static int quality()    { return clampI(Mod::get()->getSavedValue<int>("r-quality", 80), 30, 95); }
    static void setQuality(int v)    { Mod::get()->setSavedValue<int>("r-quality", clampI(v, 30, 95)); }
    static bool autoStop()  { return Mod::get()->getSavedValue<bool>("r-auto", true); }
    static void setAutoStop(bool v)  { Mod::get()->setSavedValue<bool>("r-auto", v); }
    static bool hitboxes()  { return Mod::get()->getSavedValue<bool>("r-hit", false); }
    static void setHitboxes(bool v)  { Mod::get()->setSavedValue<bool>("r-hit", v); }
    static bool audio()     { return Mod::get()->getSavedValue<bool>("r-audio", true); }
    static void setAudio(bool v)     { Mod::get()->setSavedValue<bool>("r-audio", v); }
    static bool showHud()   { return Mod::get()->getSavedValue<bool>("r-hud", true); }
    static void setShowHud(bool v)   { Mod::get()->setSavedValue<bool>("r-hud", v); }

    // ---------- status ----------
    bool rendering = false;
    bool ticked = false;          // true kalau game benar-benar jalan 1 langkah (bukan pause)
    int endCountdown = -1;        // sisa frame sebelum berhenti otomatis setelah level selesai
    int frames = 0;
    double renderFpsEma = 0.0;    // frame yang direkam per detik nyata
    double speedX = 1.0;          // kecepatan game saat render: 1.0 = normal, kurang = melambat karena lag
    std::vector<kbvid::AudioSeg> segs;   // potongan musik per attempt
    std::string songFile;         // file lagu sementara
    bool audioAdded = false;
    int outW = 0, outH = 0;

    // hasil terakhir
    std::string lastFile;
    int lastFrames = 0;
    double lastSeconds = 0.0;

    // ---------- folder & nama ----------
    static std::filesystem::path dir() {
        auto p = Mod::get()->getSaveDir() / "renders";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }

    static std::string sanitize(std::string const& name) {
        std::string out;
        for (char c : name) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') out += c;
            else if (c == ' ') out += '_';
        }
        if (out.empty()) out = "render";
        return out;
    }

    static std::string timestamp() {
        std::time_t t = std::time(nullptr);
        std::tm tmv{};
#ifdef _WIN32
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
        return buf;
    }

    // Daftar file hasil render, yang terbaru di paling atas
    static std::vector<std::filesystem::path> files() {
        std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> items;
        std::error_code ec;
        for (auto const& e : std::filesystem::directory_iterator(dir(), ec)) {
            auto ext = e.path().extension();
            if (ext != ".avi" && ext != ".mp4") continue;
            std::error_code ec2;
            items.push_back({std::filesystem::last_write_time(e.path(), ec2), e.path()});
        }
        std::sort(items.begin(), items.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
        std::vector<std::filesystem::path> out;
        for (auto const& it : items) out.push_back(it.second);
        return out;
    }

    static std::string sizeText(std::filesystem::path const& p) {
        std::error_code ec;
        auto sz = std::filesystem::file_size(p, ec);
        if (ec) return "?";
        char buf[32];
        double mb = static_cast<double>(sz) / (1024.0 * 1024.0);
        if (mb >= 1024.0) std::snprintf(buf, sizeof(buf), "%.2f GB", mb / 1024.0);
        else std::snprintf(buf, sizeof(buf), "%.1f MB", mb);
        return buf;
    }

    // ---------- pengatur kecepatan ----------
    // Mengembalikan berapa langkah game yang harus dijalankan pada frame layar ini.
    // Kalau tidak lag: kecepatan normal (1.0x). Kalau lag: game melambat supaya video tetap mulus.
    int planSteps(float realDt) {
        double step = 1.0 / static_cast<double>(fps());
        double d = static_cast<double>(realDt);
        if (d > 0.25) d = 0.25;
        if (d < 0.0) d = 0.0;
        acc += d;
        int n = static_cast<int>(acc / step);
        if (n > 2) {
            n = 2;
            acc = 0.0;
        } else {
            acc -= n * step;
        }
        winReal += d;
        if (winReal >= 0.5) {
            double sp = winGame / winReal;
            if (sp > 1.5) sp = 1.5;
            speedX = speedX * 0.5 + sp * 0.5;
            renderFpsEma = static_cast<double>(winFrames) / winReal;
            winReal = 0.0;
            winGame = 0.0;
            winFrames = 0;
        }
        return n;
    }

    void addGameTime(double sec) { winGame += sec; }

    // Catat awal potongan musik (dipanggil tiap level di-reset saat merender)
    void markSegment(double songSec) {
        if (!rendering) return;
        double vs = static_cast<double>(frames) / static_cast<double>((std::max)(1, fps()));
        if (!segs.empty() && std::fabs(segs.back().videoSec - vs) < 1e-6) {
            segs.back().songSec = songSec;
        } else {
            segs.push_back({vs, songSec});
        }
    }

    // Decode lagu jadi PCM 16-bit memakai FMOD (mesin audio game sendiri)
    static bool decodeWithFmod(std::string const& file, double maxSec, std::vector<int16_t>& pcm,
                               int& rate, int& ch, std::string& err) {
        auto* eng = FMODAudioEngine::get();
        if (!eng || !eng->m_system) {
            err = "FMOD belum siap";
            return false;
        }
        FMOD::Sound* snd = nullptr;
        FMOD_RESULT res = eng->m_system->createSound(file.c_str(), FMOD_OPENONLY | FMOD_ACCURATETIME, nullptr, &snd);
        if (res != FMOD_OK || !snd) {
            err = "FMOD createSound=" + std::to_string(static_cast<int>(res));
            return false;
        }
        FMOD_SOUND_TYPE type = FMOD_SOUND_TYPE_UNKNOWN;
        FMOD_SOUND_FORMAT fmt = FMOD_SOUND_FORMAT_NONE;
        int channels = 0, bits = 0;
        snd->getFormat(&type, &fmt, &channels, &bits);
        float freq = 44100.f;
        int prio = 0;
        snd->getDefaults(&freq, &prio);
        size_t bps = 0;
        if (fmt == FMOD_SOUND_FORMAT_PCM16) bps = 2;
        else if (fmt == FMOD_SOUND_FORMAT_PCMFLOAT) bps = 4;
        else if (fmt == FMOD_SOUND_FORMAT_PCM32) bps = 4;
        else if (fmt == FMOD_SOUND_FORMAT_PCM24) bps = 3;
        else if (fmt == FMOD_SOUND_FORMAT_PCM8) bps = 1;
        if (bps == 0 || channels < 1 || freq < 8000.f) {
            err = "FMOD format " + std::to_string(static_cast<int>(fmt)) + " ch " + std::to_string(channels);
            snd->release();
            return false;
        }
        size_t maxBytes = static_cast<size_t>(maxSec * freq) * channels * bps;
        std::vector<uint8_t> raw;
        std::vector<uint8_t> chunk(65536);
        while (raw.size() < maxBytes) {
            unsigned int got = 0;
            FMOD_RESULT rr = snd->readData(chunk.data(), static_cast<unsigned int>(chunk.size()), &got);
            if (got > 0) raw.insert(raw.end(), chunk.begin(), chunk.begin() + got);
            if (rr != FMOD_OK || got == 0) break;
        }
        snd->release();
        if (raw.empty()) {
            err = "FMOD tidak menghasilkan data";
            return false;
        }
        size_t count = raw.size() / bps;
        pcm.resize(count);
        for (size_t i = 0; i < count; i++) {
            uint8_t const* p = raw.data() + i * bps;
            int v = 0;
            if (fmt == FMOD_SOUND_FORMAT_PCM16) {
                v = static_cast<int16_t>(p[0] | (p[1] << 8));
            } else if (fmt == FMOD_SOUND_FORMAT_PCMFLOAT) {
                float f;
                std::memcpy(&f, p, 4);
                int q = static_cast<int>(f * 32767.f);
                v = q > 32767 ? 32767 : (q < -32768 ? -32768 : q);
            } else if (fmt == FMOD_SOUND_FORMAT_PCM32) {
                int32_t x;
                std::memcpy(&x, p, 4);
                v = x >> 16;
            } else if (fmt == FMOD_SOUND_FORMAT_PCM24) {
                int32_t x = (p[0] << 8) | (p[1] << 16) | (p[2] << 24);
                v = x >> 16;
            } else {
                v = (static_cast<int>(p[0]) - 128) << 8;
            }
            pcm[i] = static_cast<int16_t>(v);
        }
        rate = static_cast<int>(freq);
        ch = channels;
        return true;
    }

    // Siapkan file lagu level ini (file sementara). Mengembalikan "" kalau tidak ketemu.
    static inline std::string audioLog;   // catatan diagnosis audio

    static std::string prepareSong() {
        audioLog.clear();
        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_level) return "";
        std::vector<std::string> cands;
        int songID = pl->m_level->m_songID;
        // 1) file yang benar-benar dipakai FMOD untuk musik level
        if (auto* fmod = FMODAudioEngine::get()) {
            std::string idStr = songID > 0 ? std::to_string(songID) : std::string();
            std::vector<std::string> byId, byCh;
            for (auto& kv : fmod->m_fmodMusic) {
                std::string p = kv.second.m_filePath;
                if (p.empty() || p.find("menuLoop") != std::string::npos) continue;
                std::string const prefix = "file:///android_asset/";
                if (p.compare(0, prefix.size(), prefix) == 0) p = p.substr(prefix.size());
                if (!idStr.empty() && p.find(idStr) != std::string::npos) byId.push_back(p);
                else if (kv.second.m_channelID == 0) byCh.push_back(p);
            }
            for (auto& p : byId) cands.push_back(p);
            for (auto& p : byCh) cands.push_back(p);
        }
        if (songID > 0) {
            std::string id = std::to_string(songID);
            std::string wp = CCFileUtils::sharedFileUtils()->getWritablePath();
            if (!wp.empty() && wp.back() != '/') wp += "/";
            cands.push_back(wp + id + ".mp3");
            cands.push_back(wp + id + ".ogg");
            cands.push_back(id + ".mp3");
        } else {
            static char const* names[] = {
                "StereoMadness", "BackOnTrack", "Polargeist", "DryOut", "BaseAfterBase", "CantLetGo",
                "Jumper", "TimeMachine", "Cycles", "xStep", "Clutterfunk", "TheoryOfEverything",
                "Electroman", "Clubstep", "Electrodynamix", "HexagonForce", "BlastProcessing",
                "TheoryOfEverything2", "GeometricalDominator", "Deadlocked", "Fingerdash", "Dash"
            };
            int t = pl->m_level->m_audioTrack;
            if (t >= 0 && t < static_cast<int>(sizeof(names) / sizeof(names[0]))) {
                cands.push_back(std::string(names[t]) + ".mp3");
                cands.push_back(std::string(names[t]) + ".ogg");
            }
        }
        for (auto const& c : cands) audioLog += "cari: " + c + "\n";
        for (auto const& c : cands) {
            unsigned long size = 0;
            unsigned char* data = CCFileUtils::sharedFileUtils()->getFileData(c.c_str(), "rb", &size);
            if (!data || size == 0) {
                delete[] data;
                continue;
            }
            auto out = dir() / ".song.tmp";
            FILE* f = std::fopen(out.string().c_str(), "wb");
            bool ok = false;
            if (f) {
                ok = std::fwrite(data, 1, size, f) == size;
                std::fclose(f);
            }
            delete[] data;
            if (ok) {
                audioLog += "dipakai: " + c + " (" + std::to_string(size) + " byte)\n";
                return out.string();
            }
        }
        audioLog += "lagu tidak ditemukan\n";
        return "";
    }

    // ---------- mulai / berhenti ----------
    bool start(std::string const& levelName) {
        if (rendering) return true;
        auto* dirp = CCDirector::get();
        if (!dirp) return false;
        CCSize px = dirp->getWinSizeInPixels();
        int sw = static_cast<int>(px.width), sh = static_cast<int>(px.height);
        if (sw < 16 || sh < 16) return false;

        int oh = heightP();
        if (oh > sh) oh = sh;
        oh &= ~1;
        int ow = static_cast<int>(std::lround(static_cast<double>(sw) * oh / sh));
        ow &= ~1;
#ifdef GEODE_IS_ANDROID
        ow &= ~15;
        oh &= ~15;
#endif
        if (ow < 16 || oh < 16) return false;

        auto base = dir() / (sanitize(levelName) + "_" + timestamp());
        useMp4 = false;
        std::filesystem::path path;
#ifdef GEODE_IS_ANDROID
        {
            double bpp = 0.04 + 0.14 * (static_cast<double>(quality()) / 100.0);
            int br = static_cast<int>(static_cast<double>(ow) * oh * fps() * bpp);
            if (br < 400000) br = 400000;
            path = base;
            path += ".mp4";
            if (mp4.open(path.string(), ow, oh, fps(), br)) useMp4 = true;
        }
#endif
        if (!useMp4) {
            path = base;
            path += ".avi";
            if (!avi.open(path.string(), ow, oh, fps())) return false;
#ifdef GEODE_IS_ANDROID
            Notification::create("MP4 tidak didukung HP ini, pakai AVI", NotificationIcon::Warning)->show();
#endif
        }
        curPath = path.string();
        outW = ow;
        outH = oh;
        frames = 0;
        endCountdown = -1;
        renderFpsEma = 0.0;
        speedX = 1.0;
        acc = 0.0;
        winReal = winGame = 0.0;
        winFrames = 0;
        segs.clear();
        audioAdded = false;
        audioLog.clear();
        songFile = (useMp4 && audio()) ? prepareSong() : "";
        {
            double off = 0.0;
            auto* pl2 = PlayLayer::get();
            if (pl2 && pl2->m_levelSettings) off = static_cast<double>(pl2->m_levelSettings->m_songOffset);
            segs.push_back({0.0, off});
        }
        ticked = false;
        startClock = std::chrono::steady_clock::now();
        lastCapture = startClock;
        rendering = true;
        return true;
    }

    void stop(bool notify = true) {
        if (!rendering) return;
        rendering = false;
        ticked = false;
        endCountdown = -1;
        int n = useMp4 ? mp4.frames() : avi.frames();
        double secs = static_cast<double>(n) / static_cast<double>(fps());
        if (useMp4) mp4.close();
        else avi.close();
        if (rt) {
            rt->release();
            rt = nullptr;
        }
        if (n > 0 && useMp4 && audio()) {
            std::string err;
            if (songFile.empty()) {
                err = "file lagu tidak ditemukan";
            } else {
                double maxSong = 0.0;
                for (auto const& sg : segs) maxSong = (std::max)(maxSong, sg.songSec);
                double need = maxSong + secs + 5.0;
                std::vector<int16_t> pcm;
                int rate = 44100, ch = 2;
                std::string e1, e2;
                bool dec = decodeWithFmod(songFile, need, pcm, rate, ch, e1);
                if (!dec) {
                    audioLog += "FMOD decode gagal: " + e1 + "\n";
                    pcm.clear();
                    dec = kbvid::decodeFile(songFile, need, pcm, rate, ch, e2);
                    if (!dec) audioLog += "MediaCodec decode gagal: " + e2 + "\n";
                }
                if (!dec) {
                    err = "decode lagu gagal (" + e1 + " / " + e2 + ")";
                } else {
                    audioLog += "decode OK: " + std::to_string(pcm.size()) + " sampel, " + std::to_string(rate) + " Hz, " + std::to_string(ch) + " ch\n";
                    audioAdded = kbvid::addAudio(curPath, pcm, rate, ch, segs, secs, err);
                }
            }
            if (!audioAdded) {
                audioLog += "GAGAL: " + err + "\n";
                if (notify) Notification::create("Tanpa suara: " + err, NotificationIcon::Warning)->show();
            } else {
                audioLog += "audio OK\n";
            }
            FILE* lf = std::fopen((dir() / "audio-log.txt").string().c_str(), "wb");
            if (lf) {
                std::fwrite(audioLog.data(), 1, audioLog.size(), lf);
                std::fclose(lf);
            }
        }
        if (!songFile.empty()) {
            std::error_code ec0;
            std::filesystem::remove(songFile, ec0);
            songFile.clear();
        }
        if (n <= 0) {
            std::error_code ec;
            std::filesystem::remove(curPath, ec);
            if (notify) Notification::create("Tidak ada frame yang terekam", NotificationIcon::Warning)->show();
            return;
        }
        lastFile = curPath;
        lastFrames = n;
        lastSeconds = secs;
        if (notify) {
            auto name = std::filesystem::path(curPath).filename().string();
            Notification::create("Video tersimpan: " + name, NotificationIcon::Success)->show();
        }
    }

    // ---------- ambil 1 frame ----------
    // Dipanggil setelah game berjalan 1 langkah (waktu dikunci 1/FPS).
    void captureFrame() {
        if (!rendering) return;
        auto* dirp = CCDirector::get();
        auto* scene = dirp ? dirp->getRunningScene() : nullptr;
        if (!scene) return;

        CCSize px = dirp->getWinSizeInPixels();
        int sw = static_cast<int>(px.width), sh = static_cast<int>(px.height);
        if (!rt || rtW != sw || rtH != sh) {
            if (rt) rt->release();
            // CCRenderTexture::create memakai ukuran dalam "point"; sesuaikan supaya jumlah pikselnya = layar
            CCSize pts = dirp->getWinSize();
            rt = CCRenderTexture::create(static_cast<int>(pts.width), static_cast<int>(pts.height));
            if (rt) {
                CCSize tex = rt->getSprite()->getTexture()->getContentSizeInPixels();
                if (tex.width > 1.f && tex.height > 1.f &&
                    (std::fabs(tex.width - px.width) > 1.f || std::fabs(tex.height - px.height) > 1.f)) {
                    rt = CCRenderTexture::create(
                        static_cast<int>(std::lround(pts.width * px.width / tex.width)),
                        static_cast<int>(std::lround(pts.height * px.height / tex.height)));
                }
            }
            if (!rt) {
                Notification::create("Gagal membuat texture render", NotificationIcon::Error)->show();
                stop(false);
                return;
            }
            rt->retain();
            rtW = sw;
            rtH = sh;
        }

        // sembunyikan indikator REC supaya tidak ikut terekam
        CCNode* hud = hudNode();
        bool hudWasVisible = hud && hud->isVisible();
        if (hud) hud->setVisible(false);

        rt->beginWithClear(0.f, 0.f, 0.f, 1.f);
        scene->visit();
        rt->end();

        if (hud) hud->setVisible(hudWasVisible);

        CCImage* img = rt->newCCImage(true);
        if (!img) return;
        unsigned char* data = img->getData();
        int iw = static_cast<int>(img->getWidth());
        int ih = static_cast<int>(img->getHeight());
        if (data && iw > 0 && ih > 0) {
            bool okw;
            if (useMp4) {
                okw = mp4.addFrame(data, iw, ih);
            } else {
                auto jpeg = kbvid::JpegEncoder::encode(data, iw, ih, outW, outH, quality());
                okw = avi.addFrame(jpeg);
            }
            if (!okw) {
                img->release();
                Notification::create("Gagal menulis video (penyimpanan penuh?)", NotificationIcon::Error)->show();
                stop(false);
                return;
            }
            frames++;
        }
        img->release();

        winFrames++;

        // berhenti otomatis setelah level selesai
        if (endCountdown >= 0) {
            endCountdown--;
            if (endCountdown <= 0) this->stop(true);
        }
    }

    // ---------- indikator REC di layar ----------
    static constexpr int HUD_TAG = 7780;

    CCNode* hudNode() {
        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_uiLayer) return nullptr;
        return pl->m_uiLayer->getChildByTag(HUD_TAG);
    }

    void updateHud() {
        CCNode* hud = hudNode();
        if (!hud) return;
        bool on = rendering && showHud();
        if (hud->isVisible() != on) hud->setVisible(on);
        if (!on) return;
        auto* label = static_cast<CCLabelBMFont*>(hud->getChildByTag(1));
        auto* speed = static_cast<CCLabelBMFont*>(hud->getChildByTag(2));
        if (!label) return;
        int secs = frames / (std::max)(1, fps());
        char buf[64];
        std::snprintf(buf, sizeof(buf), "REC %02d:%02d", secs / 60, secs % 60);
        if (std::string(label->getString()) != buf) label->setString(buf);
        if (speed) {
            std::snprintf(buf, sizeof(buf), "%.1fx %s", speedX, speedX >= 0.9 ? "lancar" : (speedX >= 0.6 ? "agak lag" : "lag"));
            if (std::string(speed->getString()) != buf) speed->setString(buf);
            ccColor3B col = speedX >= 0.9 ? ccColor3B{90, 235, 140} : (speedX >= 0.6 ? ccColor3B{255, 214, 80} : ccColor3B{255, 110, 110});
            speed->setColor(col);
        }
    }

private:
    kbvid::AviWriter avi;
    kbvid::Mp4Writer mp4;
    bool useMp4 = false;
    std::string curPath;
    CCRenderTexture* rt = nullptr;
    int rtW = 0, rtH = 0;
    std::chrono::steady_clock::time_point startClock, lastCapture;
    double acc = 0.0, winReal = 0.0, winGame = 0.0;
    int winFrames = 0;
};
