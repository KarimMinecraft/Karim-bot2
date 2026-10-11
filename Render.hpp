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
    static int heightP()    { return clampI(Mod::get()->getSavedValue<int>("r-height", 720), 240, 2160); }
    static void setHeightP(int v)    { Mod::get()->setSavedValue<int>("r-height", clampI(v, 240, 2160)); }
    static int quality()    { return clampI(Mod::get()->getSavedValue<int>("r-quality", 80), 30, 95); }
    static void setQuality(int v)    { Mod::get()->setSavedValue<int>("r-quality", clampI(v, 30, 95)); }
    static bool autoStop()  { return Mod::get()->getSavedValue<bool>("r-auto", true); }
    static void setAutoStop(bool v)  { Mod::get()->setSavedValue<bool>("r-auto", v); }
    static bool hitboxes()  { return Mod::get()->getSavedValue<bool>("r-hit", false); }
    static void setHitboxes(bool v)  { Mod::get()->setSavedValue<bool>("r-hit", v); }
    static bool showHud()   { return Mod::get()->getSavedValue<bool>("r-hud", true); }
    static void setShowHud(bool v)   { Mod::get()->setSavedValue<bool>("r-hud", v); }

    // ---------- status ----------
    bool rendering = false;
    bool ticked = false;          // true kalau game benar-benar jalan 1 langkah (bukan pause)
    int endCountdown = -1;        // sisa frame sebelum berhenti otomatis setelah level selesai
    int frames = 0;
    double renderFpsEma = 0.0;    // kecepatan render nyata (frame per detik)
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
            if (e.path().extension() != ".avi") continue;
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
        if (ow < 16 || oh < 16) return false;

        auto path = dir() / (sanitize(levelName) + "_" + timestamp() + ".avi");
        if (!avi.open(path.string(), ow, oh, fps())) {
            return false;
        }
        curPath = path.string();
        outW = ow;
        outH = oh;
        frames = 0;
        endCountdown = -1;
        renderFpsEma = 0.0;
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
        int n = avi.frames();
        double secs = static_cast<double>(n) / static_cast<double>(fps());
        avi.close();
        if (rt) {
            rt->release();
            rt = nullptr;
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
            rt = CCRenderTexture::create(sw, sh);
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
            auto jpeg = kbvid::JpegEncoder::encode(data, iw, ih, outW, outH, quality());
            if (!avi.addFrame(jpeg)) {
                img->release();
                Notification::create("Gagal menulis video (penyimpanan penuh?)", NotificationIcon::Error)->show();
                stop(false);
                return;
            }
            frames++;
        }
        img->release();

        // kecepatan render nyata
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastCapture).count();
        lastCapture = now;
        if (dt > 0.0001) {
            double inst = 1.0 / dt;
            renderFpsEma = (renderFpsEma <= 0.0) ? inst : (renderFpsEma * 0.9 + inst * 0.1);
        }

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
        if (!label) return;
        int secs = frames / (std::max)(1, fps());
        char buf[64];
        std::snprintf(buf, sizeof(buf), "REC %02d:%02d  %d frame", secs / 60, secs % 60, frames);
        if (std::string(label->getString()) != buf) label->setString(buf);
    }

private:
    kbvid::AviWriter avi;
    std::string curPath;
    CCRenderTexture* rt = nullptr;
    int rtW = 0, rtH = 0;
    std::chrono::steady_clock::time_point startClock, lastCapture;
};
