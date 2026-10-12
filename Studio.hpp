#pragma once
#include <Geode/ui/NineSlice.hpp>
#include <Geode/utils/file.hpp>
#include <functional>
#include "Render.hpp"

// ============================================================
// RENDER STUDIO v2.3 - tampilan terang: header koral, tab di atas,
// kartu putih di atas latar krem, teks besar & jelas.
// ============================================================
namespace rui {
    inline ccColor3B const BG     = {255, 244, 226};   // latar krem
    inline ccColor3B const HEAD   = {255, 90, 98};     // header koral
    inline ccColor3B const CARD   = {255, 255, 255};
    inline ccColor3B const CARD2  = {255, 232, 200};   // chip / baris
    inline ccColor3B const OFFC   = {222, 212, 202};   // tombol OFF
    inline ccColor3B const ACCENT = {255, 82, 96};
    inline ccColor3B const GREEN  = {24, 166, 104};
    inline ccColor3B const AMBER  = {226, 150, 0};
    inline ccColor3B const RED    = {232, 60, 70};
    inline ccColor3B const TEXT   = {40, 34, 64};
    inline ccColor3B const DIM    = {104, 96, 128};

    inline bool same(ccColor3B a, ccColor3B b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

    // kotak warna polos (tidak bergantung pada tekstur game supaya warnanya pasti terang)
    inline CCLayerColor* box(float w, float h, ccColor3B c, GLubyte op = 255) {
        auto n = CCLayerColor::create(ccColor4B{c.r, c.g, c.b, op}, w, h);
        n->ignoreAnchorPointForPosition(false);
        n->setAnchorPoint({0.5f, 0.5f});
        return n;
    }

    inline CCLabelBMFont* label(CCNode* parent, std::string const& text, float x, float y, float scale,
                                ccColor3B color, bool left = true, float maxW = 0.f, char const* font = "bigFont.fnt") {
        auto l = CCLabelBMFont::create(text.c_str(), font);
        if (maxW > 0.f) l->limitLabelWidth(maxW, scale, 0.1f);
        else l->setScale(scale);
        l->setColor(color);
        l->setAnchorPoint({left ? 0.f : 0.5f, 0.5f});
        l->setPosition({x, y});
        parent->addChild(l, 3);
        return l;
    }

    // teks isi (font kecil tapi jelas)
    inline CCLabelBMFont* body(CCNode* parent, std::string const& text, float x, float y, ccColor3B color = TEXT,
                               float scale = 0.72f, bool left = true, float maxW = 0.f) {
        return label(parent, text, x, y, scale, color, left, maxW, "chatFont.fnt");
    }

    inline CCNode* btnNode(std::string const& text, float w, float h, ccColor3B c, float ts = 0.5f) {
        auto node = CCNode::create();
        node->setContentSize({w, h});
        auto bg = box(w, h, c);
        bg->setPosition({w / 2, h / 2});
        node->addChild(bg, 0);
        auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        bool light = same(c, ACCENT) || same(c, GREEN) || same(c, HEAD) || same(c, RED) || same(c, AMBER);
        l->setColor(light ? ccColor3B{255, 255, 255} : TEXT);
        l->limitLabelWidth(w - 12.f, ts, 0.15f);
        l->setPosition({w / 2, h / 2 + 1.f});
        node->addChild(l, 1);
        return node;
    }

    inline void fillRect(CCDrawNode* d, float x, float y, float w, float h, ccColor3B c) {
        CCPoint p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
        ccColor4F f = {c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f};
        d->drawPolygon(p, 4, f, 0.f, f);
    }
}

class RenderStudio : public geode::Popup {
protected:
    static constexpr float PWIDTH = 420.f;
    static constexpr float PHEIGHT = 270.f;
    static constexpr float CX = 10.f;       // x awal isi
    static constexpr float CY = 10.f;       // y awal isi
    static constexpr float CWIDTH = 400.f;  // lebar isi
    static constexpr float CHEIGHT = 184.f; // tinggi isi

    PauseLayer* m_pause = nullptr;
    CCNode* m_page = nullptr;
    CCMenu* m_pageMenu = nullptr;
    CCMenu* m_tabMenu = nullptr;
    int m_tab = 0;

    // ---------- kerangka ----------
    bool init(PauseLayer* pause) {
        if (!Popup::init(PWIDTH, PHEIGHT)) return false;
        m_pause = pause;
        this->setOpacity(70);   // latar belakang tidak terlalu gelap
        if (m_bgSprite) m_bgSprite->setVisible(false);
        auto border = rui::box(PWIDTH, PHEIGHT, rui::HEAD);
        border->setPosition({PWIDTH / 2, PHEIGHT / 2});
        m_mainLayer->addChild(border, -2);
        auto bgl = rui::box(PWIDTH - 6.f, PHEIGHT - 6.f, rui::BG);
        bgl->setPosition({PWIDTH / 2, PHEIGHT / 2});
        m_mainLayer->addChild(bgl, -1);

        // header
        auto head = rui::box(PWIDTH - 12.f, 32.f, rui::HEAD);
        head->setPosition({PWIDTH / 2, PHEIGHT - 22.f});
        m_mainLayer->addChild(head, 1);
        rui::label(m_mainLayer, "RENDERING UTILS", 24.f, PHEIGHT - 22.f, 0.62f, {255, 255, 255});
        rui::label(m_mainLayer, "v2.3.1", PWIDTH - 24.f, PHEIGHT - 22.f, 0.42f, {255, 236, 226}, false);

        this->showTab(0);
        return true;
    }

    void buildTabs() {
        if (m_tabMenu) m_tabMenu->removeFromParent();
        m_tabMenu = CCMenu::create();
        m_tabMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_tabMenu, 6);

        char const* names[4] = {"RENDER", "HASIL", "OPSI", "TENTANG"};
        for (int i = 0; i < 4; i++) {
            bool sel = (i == m_tab);
            auto btn = CCMenuItemExt::createSpriteExtra(
                rui::btnNode(names[i], 96.f, 26.f, sel ? rui::ACCENT : rui::CARD2, 0.42f),
                [this, i](CCObject*) { this->showTab(i); }
            );
            btn->setPosition({57.f + static_cast<float>(i) * 102.f, PHEIGHT - 56.f});
            m_tabMenu->addChild(btn);
        }
    }

    void showTab(int idx) {
        m_tab = idx;
        if (m_page) {
            m_page->removeFromParent();
            m_page = nullptr;
        }
        m_page = CCNode::create();
        m_page->setPosition({CX, CY});
        m_page->setContentSize({CWIDTH, CHEIGHT});
        auto card = rui::box(CWIDTH, CHEIGHT, rui::CARD);
        card->setPosition({CWIDTH / 2, CHEIGHT / 2});
        m_page->addChild(card, 0);
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
        this->buildTabs();
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

    static ccColor3B speedColor(double sp) {
        return sp >= 0.9 ? rui::GREEN : (sp >= 0.6 ? rui::AMBER : rui::RED);
    }
    static char const* speedText(double sp) {
        return sp >= 0.9 ? "Lancar" : (sp >= 0.6 ? "Agak lag" : "Lag");
    }

    // ---------- halaman: Render ----------
    void buildRender() {
        auto& r = RenderManager::get();
        if (!r.rendering) {
            rui::label(m_page, "SIAP MERENDER", 16.f, CHEIGHT - 18.f, 0.55f, rui::ACCENT);
            rui::body(m_page, "Level dimulai ulang dari awal, lalu kamu main seperti biasa.", 16.f, CHEIGHT - 42.f, rui::TEXT, 0.72f, true, CWIDTH - 32.f);
            rui::body(m_page, "Video dan musik direkam. Level selesai = langsung tersimpan.", 16.f, CHEIGHT - 60.f, rui::TEXT, 0.72f, true, CWIDTH - 32.f);
            rui::body(m_page, "Berhenti di tengah? Pause > Render > STOP & SIMPAN.", 16.f, CHEIGHT - 78.f, rui::DIM, 0.72f, true, CWIDTH - 32.f);

            std::string chips[4] = {
                std::to_string(RenderManager::heightP()) + "p",
                std::to_string(RenderManager::fps()) + " FPS",
                "Kualitas " + std::to_string(RenderManager::quality()),
                RenderManager::audio() ? "Suara ON" : "Suara OFF"
            };
            float cw[4] = {64.f, 74.f, 112.f, 96.f};
            float x = 16.f;
            for (int i = 0; i < 4; i++) {
                auto chip = rui::box(cw[i], 24.f, rui::CARD2);
                chip->setPosition({x + cw[i] / 2, CHEIGHT - 106.f});
                m_page->addChild(chip, 2);
                rui::label(m_page, chips[i], x + cw[i] / 2, CHEIGHT - 106.f, 0.38f, rui::TEXT, false, cw[i] - 10.f);
                x += cw[i] + 8.f;
            }

            addButton("MULAI RENDER", CWIDTH / 2, 56.f, 270.f, 40.f, rui::ACCENT, [this]() { this->onStart(); }, 0.62f);
            rui::body(m_page, "Kalau HP lag, game melambat otomatis supaya video tetap mulus.", CWIDTH / 2, 20.f, rui::DIM, 0.66f, false, CWIDTH - 20.f);
        } else {
            auto dot = CCDrawNode::create();
            dot->drawDot({0.f, 0.f}, 6.f, ccColor4F{1.f, 0.25f, 0.3f, 1.f});
            dot->setPosition({24.f, CHEIGHT - 20.f});
            dot->runAction(CCRepeatForever::create(CCSequence::create(
                CCFadeTo::create(0.5f, 90), CCFadeTo::create(0.5f, 255), nullptr)));
            m_page->addChild(dot, 3);
            rui::label(m_page, "MEREKAM", 40.f, CHEIGHT - 20.f, 0.55f, rui::ACCENT);

            int fpsv = (std::max)(1, RenderManager::fps());
            int secs = r.frames / fpsv;
            char buf[96];
            std::snprintf(buf, sizeof(buf), "Durasi:  %02d:%02d", secs / 60, secs % 60);
            rui::body(m_page, buf, 16.f, CHEIGHT - 52.f, rui::TEXT, 0.85f);
            std::snprintf(buf, sizeof(buf), "Frame:  %d", r.frames);
            rui::body(m_page, buf, 16.f, CHEIGHT - 74.f, rui::TEXT, 0.85f);
            std::snprintf(buf, sizeof(buf), "Ukuran:  %dx%d", r.outW, r.outH);
            rui::body(m_page, buf, 16.f, CHEIGHT - 96.f, rui::TEXT, 0.85f);
            std::snprintf(buf, sizeof(buf), "Rekam:  %.0f frame/dtk", r.renderFpsEma);
            rui::body(m_page, buf, 16.f, CHEIGHT - 118.f, rui::TEXT, 0.85f);

            // indikator kecepatan
            auto sbox = rui::box(170.f, 112.f, rui::CARD2);
            sbox->setPosition({CWIDTH - 100.f, CHEIGHT - 82.f});
            m_page->addChild(sbox, 1);
            double sp = r.speedX;
            rui::label(m_page, "KECEPATAN", CWIDTH - 100.f, CHEIGHT - 44.f, 0.42f, rui::DIM, false);
            std::snprintf(buf, sizeof(buf), "%.1fx", sp);
            rui::label(m_page, buf, CWIDTH - 100.f, CHEIGHT - 72.f, 0.95f, speedColor(sp), false);
            rui::label(m_page, speedText(sp), CWIDTH - 100.f, CHEIGHT - 98.f, 0.5f, speedColor(sp), false);
            auto bar = CCDrawNode::create();
            float bw = 130.f;
            rui::fillRect(bar, 0.f, 0.f, bw, 8.f, {255, 255, 255});
            float fillW = bw * static_cast<float>((std::min)(1.0, (std::max)(0.0, sp)));
            if (fillW > 0.5f) rui::fillRect(bar, 0.f, 0.f, fillW, 8.f, speedColor(sp));
            bar->setPosition({CWIDTH - 100.f - bw / 2, CHEIGHT - 120.f});
            m_page->addChild(bar, 3);

            rui::body(m_page, "1.0x = normal. Di bawah itu = game melambat karena HP lag.", CWIDTH / 2, 62.f, rui::DIM, 0.66f, false, CWIDTH - 20.f);
            addButton("STOP & SIMPAN", CWIDTH / 2, 30.f, 270.f, 34.f, rui::GREEN, [this]() { this->onStop(); }, 0.6f);
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
        rui::label(m_page, "HASIL RENDER", 16.f, CHEIGHT - 20.f, 0.55f, rui::ACCENT);
        addButton("BUKA FOLDER", CWIDTH - 92.f, CHEIGHT - 20.f, 150.f, 28.f, rui::GREEN, [this]() {
            if (!geode::utils::file::openFolder(RenderManager::dir())) {
                Notification::create("Tidak bisa membuka folder", NotificationIcon::Warning)->show();
            }
        }, 0.42f);

        auto list = RenderManager::files();
        if (list.empty()) {
            rui::label(m_page, "Belum ada video.", CWIDTH / 2, CHEIGHT / 2 + 6.f, 0.55f, rui::TEXT, false);
            rui::body(m_page, "Buka tab RENDER lalu mulai merekam.", CWIDTH / 2, CHEIGHT / 2 - 16.f, rui::DIM, 0.8f, false);
        }
        size_t shown = (std::min)(list.size(), static_cast<size_t>(3));
        for (size_t i = 0; i < shown; i++) {
            float cy = CHEIGHT - 66.f - static_cast<float>(i) * 42.f;
            auto row = rui::box(CWIDTH - 20.f, 38.f, rui::CARD2);
            row->setPosition({CWIDTH / 2, cy});
            m_page->addChild(row, 1);

            auto path = list[i];
            rui::label(m_page, path.filename().string(), 20.f, cy + 9.f, 0.38f, rui::TEXT, true, 200.f);
            rui::body(m_page, RenderManager::sizeText(path), 20.f, cy - 9.f, rui::GREEN, 0.8f);

            addButton("BUKA", CWIDTH - 124.f, cy, 66.f, 26.f, rui::GREEN, [path]() {
                if (!geode::utils::file::openFolder(path)) {
                    geode::utils::file::openFolder(RenderManager::dir());
                }
            }, 0.42f);
            addButton("HAPUS", CWIDTH - 50.f, cy, 66.f, 26.f, rui::ACCENT, [this, path]() {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                this->showTab(1);
            }, 0.42f);
        }
        if (list.size() > shown) {
            rui::body(m_page, "+" + std::to_string(list.size() - shown) + " video lainnya ada di folder", 20.f, 10.f, rui::DIM, 0.7f);
        }
    }

    // ---------- halaman: Opsi ----------
    void addChips(char const* title, float y, std::vector<std::pair<std::string, int>> const& items,
                  int current, std::function<void(int)> set) {
        rui::label(m_page, title, 16.f, y, 0.42f, rui::TEXT, true, 100.f);
        float w = 56.f, gap = 6.f;
        float x = CWIDTH - 12.f - static_cast<float>(items.size()) * (w + gap) + gap;
        for (auto const& it : items) {
            bool sel = (it.second == current);
            int val = it.second;
            addButton(it.first, x + w / 2, y, w, 24.f, sel ? rui::ACCENT : rui::CARD2,
                      [this, set, val]() { set(val); this->showTab(2); }, 0.4f);
            x += w + gap;
        }
    }

    void addSwitch(char const* title, float y, bool value, std::function<void(bool)> onChange) {
        auto row = rui::box(CWIDTH - 20.f, 22.f, rui::CARD2);
        row->setPosition({CWIDTH / 2, y});
        m_page->addChild(row, 1);
        rui::label(m_page, title, 20.f, y, 0.38f, rui::TEXT, true, 250.f);
        addButton(value ? "ON" : "OFF", CWIDTH - 46.f, y, 60.f, 18.f, value ? rui::GREEN : rui::OFFC,
                  [this, onChange, value]() { onChange(!value); this->showTab(2); }, 0.38f);
    }

    void buildOptions() {
        addChips("Resolusi", CHEIGHT - 18.f,
                 {{"360p", 360}, {"480p", 480}, {"720p", 720}, {"1080p", 1080}},
                 RenderManager::heightP(), [](int v) { RenderManager::setHeightP(v); });

        addChips("FPS", CHEIGHT - 44.f,
                 {{"24", 24}, {"30", 30}, {"60", 60}},
                 RenderManager::fps(), [](int v) { RenderManager::setFps(v); });

        // kualitas: - nilai +
        float qy = CHEIGHT - 70.f;
        rui::label(m_page, "Kualitas", 16.f, qy, 0.42f, rui::TEXT, true, 100.f);
        auto valBg = rui::box(64.f, 24.f, rui::CARD2);
        valBg->setPosition({CWIDTH - 78.f, qy});
        m_page->addChild(valBg, 1);
        rui::label(m_page, std::to_string(RenderManager::quality()), CWIDTH - 78.f, qy, 0.45f, rui::TEXT, false);
        addButton("-", CWIDTH - 128.f, qy, 32.f, 24.f, rui::CARD2,
                  [this]() { RenderManager::setQuality(RenderManager::quality() - 5); this->showTab(2); }, 0.5f);
        addButton("+", CWIDTH - 28.f, qy, 32.f, 24.f, rui::CARD2,
                  [this]() { RenderManager::setQuality(RenderManager::quality() + 5); this->showTab(2); }, 0.5f);

        addSwitch("Rekam suara (musik level)", CHEIGHT - 98.f, RenderManager::audio(),
                  [](bool v) { RenderManager::setAudio(v); });
        addSwitch("Stop otomatis saat level selesai", CHEIGHT - 124.f, RenderManager::autoStop(),
                  [](bool v) { RenderManager::setAutoStop(v); });
        addSwitch("Tampilkan hitbox di video", CHEIGHT - 150.f, RenderManager::hitboxes(),
                  [](bool v) { RenderManager::setHitboxes(v); });
        addSwitch("Indikator REC di layar", CHEIGHT - 176.f, RenderManager::showHud(),
                  [](bool v) { RenderManager::setShowHud(v); });
    }

    // ---------- halaman: Tentang ----------
    void buildAbout() {
        rui::label(m_page, "RENDERING UTILS", 16.f, CHEIGHT - 18.f, 0.55f, rui::ACCENT);

        char const* lines[] = {
            "Rekam gameplay Geometry Dash jadi video MP4,",
            "langsung dari dalam game tanpa aplikasi tambahan.",
            "",
            "- Waktu game dikunci per frame, video selalu mulus.",
            "- Lancar = kecepatan normal. Kalau HP lag, game",
            "  melambat otomatis dan indikator (1.0x) menurun.",
            "- Musik level ikut direkam (diproses setelah selesai).",
            "- Resolusi 360p-1080p, FPS 24/30/60, kualitas diatur.",
            "- Level selesai = video langsung tersimpan.",
            "- Bisa menampilkan hitbox di dalam video."
        };
        float y = CHEIGHT - 42.f;
        for (auto const* ln : lines) {
            if (ln[0] != '\0') rui::body(m_page, ln, 16.f, y, rui::TEXT, 0.74f, true, CWIDTH - 28.f);
            y -= 15.f;
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
