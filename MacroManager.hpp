#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

// Satu entri macro. Bisa berupa klik (snap=false) atau "snapshot" posisi player (snap=true)
// yang dipakai untuk mengoreksi drift saat playback supaya akurasi tetap terjaga.
struct Click {
    int frame;
    int button;     // -1 untuk snapshot
    bool down;
    bool p1;
    bool snap;
    float px;
    float py;
    double yv;
};

class MacroManager {
public:
    enum class State { Idle, Recording, Playing };

    State state = State::Idle;
    std::vector<Click> clicks;
    size_t index = 0;          // posisi entri berikutnya saat playback
    bool injecting = false;    // true saat bot sendiri yang menekan tombol
    std::string loadedName;    // nama macro yang sedang dimuat/disimpan
    std::string levelName;     // nama level yang sedang direkam

    // ---------- Fitur bot tambahan (nyala/mati, tidak disimpan permanen) ----------
    bool acOn = false;         // Auto Clicker
    bool slopeOn = false;      // Auto Slope Wave
    bool tainted = false;      // true kalau attempt ini pernah memakai bot (dipakai Safe Mode)

    // ---------- Status tombol ----------
    bool realDown = false;     // tombol layar yang ditekan pemain (asli)
    bool gameDown = false;     // tombol yang terakhir dikirim ke game

    // ---------- Auto Slope Wave: cache & kalibrasi ----------
    std::vector<GameObject*> slopes;
    GJBaseGameLayer* slopeOwner = nullptr;
    size_t slopeObjCount = 0;
    bool calibInit = false;
    bool lastUpside = false;
    bool lastMini = false;
    double kHold = 1.0;        // perubahan y per x saat ditahan
    double kRel = -1.0;        // perubahan y per x saat dilepas
    float lastX = 0.f;
    float lastY = 0.f;
    int lastFrame = -1;
    int lastAction = -1;       // -1 = tidak ada, 0 = lepas, 1 = tahan

    static constexpr double TPS = 240.0;   // perkiraan step fisika per detik

    // ---------- Pengaturan (disimpan permanen) ----------
    static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static int acCps()      { return clampInt(Mod::get()->getSavedValue<int>("ac-cps", 30), 1, 120); }
    static void setAcCps(int v)      { Mod::get()->setSavedValue<int>("ac-cps", clampInt(v, 1, 120)); }
    static int slopeCps()   { return clampInt(Mod::get()->getSavedValue<int>("slope-cps", 60), 10, 120); }
    static void setSlopeCps(int v)   { Mod::get()->setSavedValue<int>("slope-cps", clampInt(v, 10, 120)); }
    static int slopeLook()  { return clampInt(Mod::get()->getSavedValue<int>("slope-look", 90), 30, 300); }
    static void setSlopeLook(int v)  { Mod::get()->setSavedValue<int>("slope-look", clampInt(v, 30, 300)); }
    static int snapInterval() { return clampInt(Mod::get()->getSavedValue<int>("snap-interval", 4), 1, 20); }
    static void setSnapInterval(int v) { Mod::get()->setSavedValue<int>("snap-interval", clampInt(v, 1, 20)); }

    bool drivesInput() const { return acOn || slopeOn; }

    // Safe Mode berlaku kalau bot sedang aktif ATAU attempt ini sudah tercemar bot
    bool botActive() const { return state != State::Idle || tainted || acOn || slopeOn; }

    void clearSlopeCache() {
        slopes.clear();
        slopeOwner = nullptr;
        slopeObjCount = 0;
        resetSlopeCalibration();
    }

    void resetSlopeCalibration() {
        calibInit = false;
        lastAction = -1;
        lastFrame = -1;
    }

    static MacroManager& get() {
        static MacroManager instance;
        return instance;
    }

    // ---------- Safe Mode (disimpan permanen, default ON) ----------
    static bool safe() {
        return Mod::get()->getSavedValue<bool>("safe-mode", true);
    }
    static void setSafe(bool v) {
        Mod::get()->setSavedValue<bool>("safe-mode", v);
    }

    // ---------- folder & nama file ----------
    std::filesystem::path dir() {
        auto p = Mod::get()->getSaveDir() / "macros";
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
        if (out.empty()) out = "macro";
        return out;
    }

    bool exists(std::string const& name) {
        std::error_code ec;
        return std::filesystem::exists(dir() / (name + ".kbot"), ec);
    }

    // Kalau nama sudah dipakai: bloodbath -> bloodbath_2 -> bloodbath_3 ...
    std::string uniqueName(std::string const& base) {
        if (!exists(base)) return base;
        for (int i = 2; i < 10000; i++) {
            auto n = base + "_" + std::to_string(i);
            if (!exists(n)) return n;
        }
        return base + "_x";
    }

    // ---------- simpan / load / hapus ----------
    bool save(std::string const& name) {
        std::ofstream f(dir() / (name + ".kbot"));
        if (!f) return false;
        f << "KBOT2\n";
        f << std::setprecision(9);
        for (auto const& c : clicks) {
            f << c.frame << ' ' << c.button << ' ' << (c.down ? 1 : 0) << ' ' << (c.p1 ? 1 : 0)
              << ' ' << (c.snap ? 1 : 0) << ' ' << c.px << ' ' << c.py << ' ' << c.yv << '\n';
        }
        loadedName = name;
        return true;
    }

    // Dipanggil saat level selesai / rekaman dihentikan: simpan otomatis pakai nama level.
    // Mengembalikan nama macro yang tersimpan (kosong kalau tidak ada yang disimpan).
    std::string finishRecording(std::string const& suffix = "") {
        stop();
        if (clicks.empty()) return "";
        auto name = uniqueName(sanitize(levelName) + suffix);
        if (!save(name)) return "";
        return name;
    }

    bool load(std::string const& name) {
        std::ifstream f(dir() / (name + ".kbot"));
        if (!f) return false;
        std::string header;
        std::getline(f, header);
        if (header != "KBOT2") return false;

        std::vector<Click> loaded;
        int frame, button, down, p1, snap;
        float px, py;
        double yv;
        while (f >> frame >> button >> down >> p1 >> snap >> px >> py >> yv) {
            loaded.push_back(Click{frame, button, down != 0, p1 != 0, snap != 0, px, py, yv});
        }
        clicks = std::move(loaded);
        loadedName = name;
        index = 0;
        return true;
    }

    bool remove(std::string const& name) {
        std::error_code ec;
        return std::filesystem::remove(dir() / (name + ".kbot"), ec);
    }

    std::vector<std::string> list() {
        std::vector<std::string> names;
        std::error_code ec;
        for (auto const& e : std::filesystem::directory_iterator(dir(), ec)) {
            if (e.path().extension() == ".kbot") names.push_back(e.path().stem().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    // Jumlah klik (tekan) di dalam file macro, untuk ditampilkan di daftar
    int clickCount(std::string const& name) {
        std::ifstream f(dir() / (name + ".kbot"));
        if (!f) return 0;
        std::string header;
        std::getline(f, header);
        int frame, button, down, p1, snap, count = 0;
        float px, py;
        double yv;
        while (f >> frame >> button >> down >> p1 >> snap >> px >> py >> yv) {
            if (!snap && down) count++;
        }
        return count;
    }

    int liveClickCount() const {
        return static_cast<int>(std::count_if(clicks.begin(), clicks.end(),
            [](Click const& c) { return !c.snap && c.down; }));
    }

    // ---------- logika record / playback ----------

    // Dipanggil saat level di-reset (mati di practice mode -> balik ke checkpoint).
    // Semua entri setelah frame checkpoint dibuang supaya yang tersimpan cuma run yang benar.
    void truncateFrom(int frame) {
        clicks.erase(
            std::remove_if(clicks.begin(), clicks.end(), [&](Click const& c) { return c.frame >= frame; }),
            clicks.end()
        );
        // Kalau tombol masih "ditahan" di akhir rekaman, tambahkan release di frame ini
        std::map<int, Click> held;
        for (auto const& c : clicks) {
            if (c.snap) continue;
            int key = c.button * 2 + (c.p1 ? 1 : 0);
            if (c.down) held[key] = c;
            else held.erase(key);
        }
        for (auto const& [key, c] : held) {
            clicks.push_back(Click{frame, c.button, false, c.p1, false, c.px, c.py, c.yv});
        }
    }

    // Lompat ke entri pertama yang frame-nya >= frame sekarang
    void seek(int frame) {
        index = std::lower_bound(
            clicks.begin(), clicks.end(), frame,
            [](Click const& c, int f) { return c.frame < f; }
        ) - clicks.begin();
    }

    void stop() {
        if (state != State::Idle) tainted = true;   // run ini sudah pakai bot -> Safe Mode tetap berlaku
        state = State::Idle;
        injecting = false;
    }
};
