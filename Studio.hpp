#pragma once
#include <Geode/ui/NineSlice.hpp>
#include <functional>
#include "Render.hpp"

// ============================================================
// RENDER STUDIO - tampilan baru: cerah (krem & oranye muda), aksen merah-koral,
// sidebar di kiri, isi di kanan.
// ============================================================
namespace rui {
    inline ccColor3B const BG      = {250, 246, 238};
    inline ccColor3B const SIDE    = {255, 214, 160};
    inline ccColor3B const CARD    = {255, 255, 255};
    inline ccColor3B const CARD2   = {255, 232, 196};
    inline ccColor3B const ACCENT  = {255, 82, 96};
    inline ccColor3B const TEAL    = {0, 140, 126};
    inline ccColor3B const TEXT    = {38, 32, 58};
    inline ccColor3B const DIM     = {104, 98, 126};

    inline NineSlice* box(float w, float h, ccColor3B c, GLubyte op = 255) {
        auto n = NineSlice::create("square02_001.png");
        n->setContentSize({w, h});
        n->setColor(c);
        n->setOpacity(op);
        return n;
    }

    inline CCLabelBMFont* label(CCNode* parent, std::string const& text, float x, float y, float scale,
                                ccColor3B color, bool left = true, float maxW = 0.f, char const* font = "bigFont.fnt") {
        auto l = CCLabelBMFont::create(text.c_str(), font);
        if (maxW > 0.f) l->limitLabelWidth(maxW, scale, 0.15f);
        else l->setScale(scale);
        l->setColor(color);
        l->setAnchorPoint({left ? 0.f : 0.5f, 0.5f});
        l->setPosition({x, y});
        parent->addChild(l, 3);
        return l;
    }

    inline CCNode* btnNode(std::string const& text, float w, float h, ccColor3B c, float ts = 0.5f) {
        auto node = CCNode::create();
        node->setContentSize({w, h});
        auto bg = box(w, h, c);
        bg->setPosition({w / 2, h / 2});
        node->addChild(bg, 0);
        auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        bool strong = (c.r == ACCENT.r && c.g == ACCENT.g && c.b == ACCENT.b);
        l->setColor(strong ? ccColor3B{255, 255, 255} : TEXT);
        l->limitLabelWidth(w - 12.f, ts, 0.18f);
        l->setPosition({w / 2, h / 2 + 1.f});
        node->addChild(l, 1);
        return node;
    }
}

class RenderStudio : public geode::Popup {
protected:
    static constexpr float PWIDTH = 400.f;
    static constexpr float PHEIGHT = 260.f;
    static constexpr float CX = 112.f;      // x awal isi
    static constexpr float CWIDTH = 278.f;  // lebar isi
    static constexpr float CHEIGHT = 232.f; // tinggi isi

    PauseLayer* m_pause = nullptr;
    CCNode* m_page = nullptr;
    CCMenu* m_pageMenu = nullptr;
    CCMenu* m_sideMenu = nullptr;
    int m_tab = 0;

    // ---------- kerangka ----------
    bool init(PauseLayer* pause) {
        if (!Popup::init(PWIDTH, PHEIGHT, "square02_001.png")) return false;
        m_pause = pause;
        m_bgSprite->setColor(rui::BG);
        m_bgSprite->setOpacity(250);

        // aksen atas
        auto bar = CCLayerColor::create({255, 82, 96, 255}, PWIDTH - 28.f, 2.f);
        bar->setPosition({14.f, PHEIGHT - 8.f});
        m_mainLayer->addChild(bar, 2);

        // sidebar
        auto side = rui::box(94.f, CHEIGHT, rui::SIDE);
        side->setPosition({10.f + 47.f, 14.f + CHEIGHT / 2});
        m_mainLayer->addChild(side, 1);

        rui::label(m_mainLayer, "RENDER", 57.f, PHEIGHT - 32.f, 0.5f, rui::TEXT, false);
        rui::label(m_mainLayer, "STUDIO", 57.f, PHEIGHT - 48.f, 0.42f, rui::ACCENT, false);

        this->showTab(0);
        return true;
    }

    void buildSidebar() {
        if (m_sideMenu) m_sideMenu->removeFromParent();
        m_sideMenu = CCMenu::create();
        m_sideMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_sideMenu, 6);

        char const* names[4] = {"Render", "Hasil", "Opsi", "Tentang"};
        for (int i = 0; i < 4; i++) {
            bool sel = (i == m_tab);
            auto btn = CCMenuItemExt::createSpriteExtra(
                rui::btnNode(names[i], 80.f, 28.f, sel ? rui::ACCENT : rui::CARD2, 0.42f),
                [this, i](CCObject*) { this->showTab(i); }
            );
            btn->setPosition({57.f, PHEIGHT - 82.f - static_cast<float>(i) * 36.f});
            m_sideMenu->addChild(btn);
        }
    }

    void showTab(int idx) {
        m_tab = idx;
        if (m_page) {
            m_page->removeFromParent();
            m_page = nullptr;
        }
        m_page = CCNode::create();
        m_page->setPosition({CX, 14.f});
        m_page->setContentSize({CWIDTH, CHEIGHT});
        m_pageMenu = CCMenu::create();
        m_pageMenu->setPosition({0.f, 0.f});
        m_page->addChild(m_pageMenu, 6);
        m_mainLayer->addChild(m_page, 4);

        switch (idx) {
            case 0: buildRender(); break;
            case 1: buildResults(); break;
            case 2: buildOptions(); break;
            default: buildAbout(); break;
        }
        this->buildSidebar();
    }

    CCMenuItemSpriteExtra* addButton(std::string const& text, float x, float y, float w, float h,
                                     ccColor3B c, std::function<void()> cb, float ts = 0.5f) {
        auto btn = CCMenuItemExt::createSpriteExtra(
            rui::btnNode(text, w, h, c, ts),
            [cb](CCObject*) { cb(); }
        );
        btn->setPosition({x, y});
        m_pageMenu->addChild(btn);
        return btn;
    }

    // ---------- halaman: Render ----------
    void buildRender() {
        auto& r = RenderManager::get();
        auto card = rui::box(CWIDTH, 152.f, rui::CARD);
        card->setPosition({CWIDTH / 2, CHEIGHT - 76.f});
        m_page->addChild(card, 1);

        if (!r.rendering) {
            rui::label(m_page, "SIAP MERENDER", 14.f, CHEIGHT - 20.f, 0.5f, rui::TEAL);
            rui::label(m_page, "Level dimulai ulang dari awal, lalu kamu main", 14.f, CHEIGHT - 42.f, 0.55f, rui::DIM, true, CWIDTH - 28.f, "chatFont.fnt");
            rui::label(m_page, "seperti biasa. Video direkam otomatis.", 14.f, CHEIGHT - 56.f, 0.55f, rui::DIM, true, CWIDTH - 28.f, "chatFont.fnt");
            rui::label(m_page, "Untuk menyimpan: pause > Render > Stop.", 14.f, CHEIGHT - 70.f, 0.55f, rui::DIM, true, CWIDTH - 28.f, "chatFont.fnt");

            std::string chips[3] = {
                std::to_string(RenderManager::heightP()) + "p",
                std::to_string(RenderManager::fps()) + " FPS",
                "Kualitas " + std::to_string(RenderManager::quality())
            };
            float cw[3] = {70.f, 76.f, 98.f};
            float x = 14.f;
            for (int i = 0; i < 3; i++) {
                auto chip = rui::box(cw[i], 24.f, rui::CARD2);
                chip->setPosition({x + cw[i] / 2, CHEIGHT - 108.f});
                m_page->addChild(chip, 2);
                rui::label(m_page, chips[i], x + cw[i] / 2, CHEIGHT - 108.f, 0.38f, rui::TEXT, false, cw[i] - 10.f);
                x += cw[i] + 8.f;
            }
            rui::label(m_page, "Ubah resolusi & FPS di tab Opsi", 14.f, CHEIGHT - 134.f, 0.5f, rui::DIM, true, CWIDTH - 28.f, "chatFont.fnt");

            addButton("MULAI RENDER", CWIDTH / 2, 50.f, 230.f, 38.f, rui::ACCENT, [this]() { this->onStart(); }, 0.6f);
            rui::label(m_page, "Tanpa suara. Render lebih lambat dari waktu asli itu normal.", CWIDTH / 2, 18.f, 0.5f, rui::DIM, false, CWIDTH - 12.f, "chatFont.fnt");
        } else {
            // titik merah berkedip
            auto dot = CCDrawNode::create();
            dot->drawDot({0.f, 0.f}, 5.f, ccColor4F{1.f, 0.25f, 0.3f, 1.f});
            dot->setPosition({22.f, CHEIGHT - 20.f});
            dot->runAction(CCRepeatForever::create(CCSequence::create(
                CCFadeTo::create(0.5f, 90), CCFadeTo::create(0.5f, 255), nullptr)));
            m_page->addChild(dot, 3);
            rui::label(m_page, "MEREKAM", 36.f, CHEIGHT - 20.f, 0.5f, rui::ACCENT);

            int fpsv = (std::max)(1, RenderManager::fps());
            int secs = r.frames / fpsv;
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Frame       %d", r.frames);
            rui::label(m_page, buf, 14.f, CHEIGHT - 50.f, 0.42f, rui::TEXT);
            std::snprintf(buf, sizeof(buf), "Durasi      %02d:%02d", secs / 60, secs % 60);
            rui::label(m_page, buf, 14.f, CHEIGHT - 70.f, 0.42f, rui::TEXT);
            std::snprintf(buf, sizeof(buf), "Kecepatan   %.1f frame/dtk", r.renderFpsEma);
            rui::label(m_page, buf, 14.f, CHEIGHT - 90.f, 0.42f, rui::TEXT, true, CWIDTH - 28.f);
            std::snprintf(buf, sizeof(buf), "Ukuran      %dx%d", r.outW, r.outH);
            rui::label(m_page, buf, 14.f, CHEIGHT - 110.f, 0.42f, rui::TEXT);
            rui::label(m_page, "Game sedang di-pause, perekaman berhenti sementara.", 14.f, CHEIGHT - 134.f, 0.5f, rui::DIM, true, CWIDTH - 28.f, "chatFont.fnt");

            addButton("STOP & SIMPAN", CWIDTH / 2, 50.f, 230.f, 38.f, rui::TEAL, [this]() { this->onStop(); }, 0.6f);
            rui::label(m_page, "Lanjutkan permainan lewat tombol Resume untuk terus merekam.", CWIDTH / 2, 18.f, 0.5f, rui::DIM, false, CWIDTH - 12.f, "chatFont.fnt");
        }
    }

    void onStart() {
        auto* pl = PlayLayer::get();
        if (!pl) {
            Notification::create("Buka dulu sebuah level", NotificationIcon::Warning)->show();
            return;
        }
        auto& r = RenderManager::get();
        if (!r.start(pl->m_level->m_levelName)) {
            Notification::create("Gagal memulai render (cek penyimpanan)", NotificationIcon::Error)->show();
            return;
        }
        Notification::create("Render dimulai", NotificationIcon::Success)->show();
        auto* pause = m_pause;
        this->onClose(nullptr);
        if (pause) pause->onRestartFull(nullptr);
    }

    void onStop() {
        RenderManager::get().stop(true);
        this->showTab(1);
    }

    // ---------- halaman: Hasil ----------
    void buildResults() {
        rui::label(m_page, "HASIL RENDER", 4.f, CHEIGHT - 12.f, 0.5f, rui::TEAL);

        auto list = RenderManager::files();
        if (list.empty()) {
            auto card = rui::box(CWIDTH, 96.f, rui::CARD);
            card->setPosition({CWIDTH / 2, CHEIGHT - 80.f});
            m_page->addChild(card, 1);
            rui::label(m_page, "Belum ada video.", CWIDTH / 2, CHEIGHT - 66.f, 0.45f, rui::TEXT, false);
            rui::label(m_page, "Buka tab Render lalu mulai merekam.", CWIDTH / 2, CHEIGHT - 90.f, 0.55f, rui::DIM, false, CWIDTH - 20.f, "chatFont.fnt");
        }
        size_t shown = (std::min)(list.size(), static_cast<size_t>(4));
        for (size_t i = 0; i < shown; i++) {
            float cy = CHEIGHT - 52.f - static_cast<float>(i) * 40.f;
            auto row = rui::box(CWIDTH, 36.f, rui::CARD);
            row->setPosition({CWIDTH / 2, cy});
            m_page->addChild(row, 1);

            std::string name = list[i].filename().string();
            rui::label(m_page, name, 12.f, cy + 7.f, 0.4f, rui::TEXT, true, 176.f);
            rui::label(m_page, RenderManager::sizeText(list[i]), 12.f, cy - 8.f, 0.55f, rui::TEAL, true, 0.f, "chatFont.fnt");

            auto path = list[i];
            addButton("Hapus", CWIDTH - 40.f, cy, 62.f, 24.f, rui::ACCENT, [this, path]() {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                this->showTab(1);
            }, 0.4f);
        }
        if (list.size() > shown) {
            rui::label(m_page, "+" + std::to_string(list.size() - shown) + " video lainnya di folder", 4.f, 44.f, 0.5f, rui::DIM, true, CWIDTH - 8.f, "chatFont.fnt");
        }

        rui::label(m_page, "Folder:", 4.f, 26.f, 0.5f, rui::DIM, true, 0.f, "chatFont.fnt");
        rui::label(m_page, RenderManager::dir().string(), 4.f, 12.f, 0.5f, rui::TEXT, true, CWIDTH - 8.f, "chatFont.fnt");
    }

    // ---------- halaman: Opsi ----------
    void addChips(char const* title, float y, std::vector<std::pair<std::string, int>> const& items,
                  int current, std::function<void(int)> set) {
        rui::label(m_page, title, 4.f, y, 0.42f, rui::TEXT, true, 70.f);
        float w = 44.f, gap = 5.f;
        float x = CWIDTH - 4.f - static_cast<float>(items.size()) * (w + gap) + gap;
        for (auto const& it : items) {
            bool sel = (it.second == current);
            int val = it.second;
            addButton(it.first, x + w / 2, y, w, 24.f, sel ? rui::ACCENT : rui::CARD2,
                      [this, set, val]() { set(val); this->showTab(2); }, 0.38f);
            x += w + gap;
        }
    }

    void addSwitch(char const* title, float y, bool value, std::function<void(bool)> onChange) {
        auto row = rui::box(CWIDTH, 28.f, rui::CARD);
        row->setPosition({CWIDTH / 2, y});
        m_page->addChild(row, 1);
        rui::label(m_page, title, 12.f, y, 0.38f, rui::TEXT, true, CWIDTH - 90.f);
        addButton(value ? "ON" : "OFF", CWIDTH - 34.f, y, 52.f, 20.f, value ? rui::TEAL : rui::CARD2,
                  [this, onChange, value]() { onChange(!value); this->showTab(2); }, 0.38f);
    }

    void buildOptions() {
        rui::label(m_page, "PENGATURAN", 4.f, CHEIGHT - 12.f, 0.5f, rui::TEAL);

        addChips("Resolusi", CHEIGHT - 42.f,
                 {{"360p", 360}, {"480p", 480}, {"720p", 720}, {"1080p", 1080}},
                 RenderManager::heightP(), [](int v) { RenderManager::setHeightP(v); });

        addChips("FPS", CHEIGHT - 74.f,
                 {{"24", 24}, {"30", 30}, {"60", 60}},
                 RenderManager::fps(), [](int v) { RenderManager::setFps(v); });

        // kualitas: - nilai +
        rui::label(m_page, "Kualitas", 4.f, CHEIGHT - 106.f, 0.42f, rui::TEXT, true, 70.f);
        auto valBg = rui::box(60.f, 24.f, rui::CARD2);
        valBg->setPosition({CWIDTH - 70.f, CHEIGHT - 106.f});
        m_page->addChild(valBg, 1);
        rui::label(m_page, std::to_string(RenderManager::quality()), CWIDTH - 70.f, CHEIGHT - 106.f, 0.45f, rui::TEXT, false);
        addButton("-", CWIDTH - 118.f, CHEIGHT - 106.f, 30.f, 24.f, rui::CARD2,
                  [this]() { RenderManager::setQuality(RenderManager::quality() - 5); this->showTab(2); }, 0.5f);
        addButton("+", CWIDTH - 22.f, CHEIGHT - 106.f, 30.f, 24.f, rui::CARD2,
                  [this]() { RenderManager::setQuality(RenderManager::quality() + 5); this->showTab(2); }, 0.5f);

        addSwitch("Stop otomatis saat level selesai", CHEIGHT - 140.f, RenderManager::autoStop(),
                  [](bool v) { RenderManager::setAutoStop(v); });
        addSwitch("Tampilkan hitbox di video", CHEIGHT - 172.f, RenderManager::hitboxes(),
                  [](bool v) { RenderManager::setHitboxes(v); });
        addSwitch("Indikator REC di layar", CHEIGHT - 204.f, RenderManager::showHud(),
                  [](bool v) { RenderManager::setShowHud(v); });
    }

    // ---------- halaman: Tentang ----------
    void buildAbout() {
        auto card = rui::box(CWIDTH, CHEIGHT, rui::CARD);
        card->setPosition({CWIDTH / 2, CHEIGHT / 2});
        m_page->addChild(card, 1);

        rui::label(m_page, "RENDERING UTILS", 14.f, CHEIGHT - 18.f, 0.52f, rui::ACCENT);
        rui::label(m_page, "v2.2 - rekam gameplay jadi video", 14.f, CHEIGHT - 36.f, 0.5f, rui::DIM, true, CWIDTH - 28.f, "chatFont.fnt");

        char const* lines[] = {
            "Merekam gameplay Geometry Dash langsung menjadi",
            "file video (.mp4) tanpa aplikasi tambahan.",
            "",
            "- Waktu game dikunci per frame, jadi video selalu",
            "  mulus walau HP-mu tidak sekuat itu.",
            "- Pilih resolusi 360p sampai 1080p, FPS 24/30/60,",
            "  dan kualitas video.",
            "- Stop otomatis saat level selesai.",
            "- Bisa menampilkan hitbox di dalam video.",
            "- Hasil bisa diputar di VLC atau MX Player.",
            "",
            "Catatan: tanpa suara, render lebih lambat dari",
            "waktu asli, dan file video berukuran besar."
        };
        float y = CHEIGHT - 58.f;
        for (auto const* ln : lines) {
            if (ln[0] != '\0') rui::label(m_page, ln, 14.f, y, 0.55f, rui::TEXT, true, CWIDTH - 24.f, "chatFont.fnt");
            y -= 14.f;
        }
    }

public:
    static RenderStudio* create(PauseLayer* pause) {
        auto ret = new RenderStudio();
        if (ret->init(pause)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
