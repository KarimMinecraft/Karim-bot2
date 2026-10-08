#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

using namespace geode::prelude;

// Satu event klik: di frame berapa, tombol apa, ditekan/dilepas, player 1/2
struct Click {
    int frame;
    int button;
    bool down;
    bool p1;
};

class MacroManager {
public:
    enum class State { Idle, Recording, Playing };

    State state = State::Idle;
    std::vector<Click> clicks;
    size_t index = 0;          // posisi klik berikutnya saat playback
    bool injecting = false;    // true saat bot sendiri yang menekan tombol
    std::string loadedName;

    static MacroManager& get() {
        static MacroManager instance;
        return instance;
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

    // ---------- simpan / load / hapus ----------
    bool save(std::string const& rawName) {
        auto name = sanitize(rawName);
        std::ofstream f(dir() / (name + ".kbot"));
        if (!f) return false;
        f << "KBOT1\n";
        for (auto const& c : clicks) {
            f << c.frame << ' ' << c.button << ' ' << (c.down ? 1 : 0) << ' ' << (c.p1 ? 1 : 0) << '\n';
        }
        loadedName = name;
        return true;
    }

    bool load(std::string const& name) {
        std::ifstream f(dir() / (name + ".kbot"));
        if (!f) return false;
        std::string header;
        std::getline(f, header);
        if (header != "KBOT1") return false;

        std::vector<Click> loaded;
        int frame, button, down, p1;
        while (f >> frame >> button >> down >> p1) {
            loaded.push_back({frame, button, down != 0, p1 != 0});
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

    // ---------- logika record / playback ----------

    // Dipanggil saat level di-reset (mati di practice mode -> balik ke checkpoint).
    // Semua klik setelah frame checkpoint dibuang supaya yang tersimpan cuma run yang benar.
    void truncateFrom(int frame) {
        clicks.erase(
            std::remove_if(clicks.begin(), clicks.end(), [&](Click const& c) { return c.frame >= frame; }),
            clicks.end()
        );
        // Kalau tombol masih "ditahan" di akhir rekaman, tambahkan release di frame ini
        std::map<int, Click> held;
        for (auto const& c : clicks) {
            int key = c.button * 2 + (c.p1 ? 1 : 0);
            if (c.down) held[key] = c;
            else held.erase(key);
        }
        for (auto const& [key, c] : held) {
            clicks.push_back({frame, c.button, false, c.p1});
        }
    }

    // Lompat ke klik pertama yang frame-nya >= frame sekarang
    void seek(int frame) {
        index = std::lower_bound(
            clicks.begin(), clicks.end(), frame,
            [](Click const& c, int f) { return c.frame < f; }
        ) - clicks.begin();
    }

    void stop() {
        state = State::Idle;
        injecting = false;
    }
};
