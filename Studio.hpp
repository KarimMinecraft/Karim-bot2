#pragma once
#include <Geode/utils/file.hpp>
#include <functional>
#include "Render.hpp"

// ============================================================
// RENDER STUDIO v4 - minimalis: satu tombol rekam besar di tengah,
// pengaturan / video / tentang ada di layar terpisah.
// Semua digambar sendiri (tanpa tekstur game), warna terang.
// ============================================================
namespace rui {
    inline ccColor3B const BG     = {236, 242, 255};
    inline ccColor3B const WHITE  = {255, 255, 255};
    inline ccColor3B const BLUE   = {58, 110, 255};
    inline ccColor3B const SOFT   = {226, 233, 250};   // tombol tak terpilih
    inline ccColor3B const REC    = {240, 64, 84};
    inline ccColor3B const RECSOFT= {255, 226, 231};
    inline ccColor3B const GREEN  = {24, 170, 120};
    inline ccColor3B const AMBER  = {232, 150, 0};
    inline ccColor3B const RED    = {232, 60, 70};
    inline ccColor3B const TEXT   = {28, 36, 64};
    inline ccColor3B const DIM    = {106, 118, 152};

    inline bool same(ccColor3B a, ccColor3B b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
    inline ccColor4F f4(ccColor3B c) { return ccColor4F{c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f}; }

    inline void fillRect(CCDrawNode* d, float x, float y, float w, float h, ccColor3B c) {
        CCPoint p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
        d->drawPolygon(p, 4, f4(c), 0.f, f4(c));
    }

    // kotak dengan sudut membulat
    inline void fillRound(CCDrawNode* d, float x, float y, float w, float h, float r, ccColor3B c) {
        r = (std::min)(r, (std::min)(w, h) / 2.f);
        fillRect(d, x + r, y, w - 2 * r, h, c);
        fillRect(d, x, y + r, w, h - 2 * r, c);
        d->drawDot({x + r, y + r}, r, f4(c));
        d->drawDot({x + w - r, y + r}, r, f4(c));
        d->drawDot({x + r, y + h - r}, r, f4(c));
        d->drawDot({x + w - r, y + h - r}, r, f4(c));
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

    inline CCLabelBMFont* body(CCNode* parent, std::string const& text, float x, float y, ccColor3B color = TEXT,
                               float scale = 0.72f, bool left = true, float maxW = 0.f) {
        return label(parent, text, x, y, scale, color, left, maxW, "chatFont.fnt");
    }

    // tombol pil dengan teks
    inline CCNode* pill(std::string const& text, float w, float h, ccColor3B fill, ccColor3B textColor, float ts = 0.4f) {
        auto node = CCNode::create();
        node->setContentSize({w, h});
        auto d = CCDrawNode::create();
        fillRound(d, 0.f, 0.f, w, h, h / 2.f, fill);
        node->addChild(d, 0);
        auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        l->setColor(textColor);
        l->limitLabelWidth(w - 10.f, ts, 0.12f);
        l->setPosition({w / 2, h / 2 + 1.f});
        node->addChild(l, 1);
        return node;
    }

    // saklar ON/OFF
    inline CCNode* toggle(bool on) {
        auto node = CCNode::create();
        node->setContentSize({36.f, 18.f});
        auto d = CCDrawNode::create();
        fillRound(d, 0.f, 0.f, 36.f, 18.f, 9.f, on ? GREEN : ccColor3B{196, 204, 224});
        d->drawDot({on ? 27.f : 9.f, 9.f}, 7.f, f4(WHITE));
        node->addChild(d, 0);
        return node;
    }
}

class RenderStudio : public geode::Popup {
protected:
    static constexpr float PW = 360.f;
    static constexpr float PH = 300.f;
    enum class Page { Main, Settings, Videos, Info };

    PauseLayer* m_pause = nullptr;
    CCNode* m_root = nullptr;
    CCMenu* m_menu = nullptr;
    Page m_page = Page::Main;

    bool init(PauseLayer* pause) {
        if (!Popup::init(PW, PH)) return false;
        m_pause = pause;
        this->setOpacity(70);
        if (m_bgSprite) m_bgSprite->setVisible(false);

        auto d = CCDrawNode::create();
        rui::fillRound(d, 0.f, 0.f, PW, PH, 22.f, ccColor3B{206, 218, 246});
        rui::fillRound(d, 2.f, 2.f, PW - 4.f, PH - 4.f, 20.f, rui::WHITE);
        m_mainLayer->addChild(d, -1);

        this->build();
        return true;
    }

    CCMenuItemSpriteExtra* addItem(CCNode* node, float x, float y, std::function<void()> cb) {
        auto btn = CCMenuItemExt::createSpriteExtra(node, [cb](CCObject*) { cb(); });
        btn->setPosition({x, y});
        m_menu->addChild(btn);
        return btn;
    }

    CCMenuItemSpriteExtra* addPill(std::string const& text, float cx, float cy, float w, float h,
                                   ccColor3B fill, ccColor3B tc, std::function<void()> cb, float ts = 0.4f) {
        return addItem(rui::pill(text, w, h, fill, tc, ts), cx, cy, cb);
    }

    static ccColor3B speedColor(double sp) {
        return sp >= 0.9 ? rui::GREEN : (sp >= 0.6 ? rui::AMBER : rui::RED);
    }
    static char const* speedText(double sp) {
        return sp >= 0.9 ? "lancar" : (sp >= 0.6 ? "agak lag" : "lag");
    }

    void go(Page p) {
        m_page = p;
        this->refresh();
    }

    // dijalankan di frame berikutnya supaya tombol yang sedang ditekan tidak dihapus saat masih aktif
    void refresh() {
        geode::Loader::get()->queueInMainThread([self = geode::Ref<RenderStudio>(this)]() { self->build(); });
    }

    void build() {
        if (m_root) m_root->removeFromParent();
        m_root = CCNode::create();
        m_mainLayer->addChild(m_root, 4);
        auto shapes = CCDrawNode::create();
        m_root->addChild(shapes, 0);
        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_root->addChild(m_menu, 10);

        switch (m_page) {
            case Page::Main: buildMain(); break;
            case Page::Settings: buildSettings(shapes); break;
            case Page::Videos: buildVideos(shapes); break;
            default: buildInfo(); break;
        }
    }

    // judul + tombol kembali untuk layar selain utama
    void header(char const* title) {
        addPill("< Kembali", 56.f, PH - 26.f, 82.f, 24.f, rui::SOFT, rui::TEXT,
                [this]() { this->go(Page::Main); }, 0.34f);
        rui::label(m_root, title, PW / 2 + 30.f, PH - 26.f, 0.48f, rui::TEXT, false);
    }

    // ---------- layar utama ----------
    void buildMain() {
        auto& r = RenderManager::get();
        rui::label(m_root, "Rendering Utils", PW / 2, PH - 30.f, 0.55f, rui::TEXT, false);
        std::string sum = std::to_string(RenderManager::heightP()) + "p  -  " +
                          std::to_string(RenderManager::fps()) + " FPS  -  " +
                          (RenderManager::audio() ? "dengan suara" : "tanpa suara");
        rui::body(m_root, sum, PW / 2, PH - 52.f, rui::DIM, 0.8f, false);

        // tombol rekam besar
        auto node = CCNode::create();
        node->setContentSize({116.f, 116.f});
        auto d = CCDrawNode::create();
        d->drawDot({58.f, 58.f}, 56.f, rui::f4(rui::RECSOFT));
        d->drawDot({58.f, 58.f}, 44.f, rui::f4(rui::REC));
        if (r.rendering) rui::fillRound(d, 58.f - 15.f, 58.f - 15.f, 30.f, 30.f, 7.f, rui::WHITE);
        else d->drawDot({58.f, 58.f}, 16.f, rui::f4(rui::WHITE));
        node->addChild(d);
        addItem(node, PW / 2, 150.f, [this]() { this->onRecordButton(); });

        // status
        if (r.rendering) {
            int secs = r.frames / (std::max)(1, RenderManager::fps());
            char buf[96];
            std::snprintf(buf, sizeof(buf), "REC %02d:%02d   %.1fx %s", secs / 60, secs % 60, r.speedX, speedText(r.speedX));
            rui::label(m_root, buf, PW / 2, 78.f, 0.42f, speedColor(r.speedX), false);
            rui::body(m_root, "Ketuk tombol untuk berhenti dan simpan", PW / 2, 58.f, rui::DIM, 0.75f, false);
        } else {
            rui::label(m_root, "Siap merekam", PW / 2, 78.f, 0.42f, rui::TEXT, false);
            rui::body(m_root, "Ketuk tombol merah untuk mulai", PW / 2, 58.f, rui::DIM, 0.75f, false);
        }

        addPill("Video", PW / 2 - 70.f, 26.f, 116.f, 28.f, rui::SOFT, rui::TEXT, [this]() { this->go(Page::Videos); }, 0.4f);
        addPill("Pengaturan", PW / 2 + 70.f, 26.f, 116.f, 28.f, rui::SOFT, rui::TEXT, [this]() { this->go(Page::Settings); }, 0.36f);
    }

    // ---------- layar pengaturan ----------
    void buildSettings(CCDrawNode* shapes) {
        this->header("Pengaturan");

        float left = 28.f, right = PW - 28.f;
        float y = PH - 68.f;
        float step = 34.f;

        auto divider = [&](float yy) { rui::fillRect(shapes, left, yy, right - left, 1.f, ccColor3B{236, 240, 250}); };

        auto segRow = [&](char const* title, std::vector<std::pair<std::string, int>> const& items,
                          int current, std::function<void(int)> set) {
            rui::label(m_root, title, left, y, 0.42f, rui::TEXT);
            float w = 48.f, gap = 4.f;
            float x = right - static_cast<float>(items.size()) * (w + gap) + gap;
            for (auto const& it : items) {
                bool sel = (it.second == current);
                int val = it.second;
                addPill(it.first, x + w / 2, y, w, 24.f, sel ? rui::BLUE : rui::SOFT, sel ? rui::WHITE : rui::TEXT,
                        [this, set, val]() { set(val); this->refresh(); }, 0.34f);
                x += w + gap;
            }
            y -= step;
            divider(y + step / 2);
        };
        segRow("Resolusi", {{"360p", 360}, {"480p", 480}, {"720p", 720}, {"1080p", 1080}},
               RenderManager::heightP(), [](int v) { RenderManager::setHeightP(v); });
        segRow("FPS", {{"24", 24}, {"30", 30}, {"60", 60}},
               RenderManager::fps(), [](int v) { RenderManager::setFps(v); });

        // kualitas
        rui::label(m_root, "Kualitas", left, y, 0.42f, rui::TEXT);
        addPill("-", right - 92.f, y, 28.f, 24.f, rui::SOFT, rui::TEXT,
                [this]() { RenderManager::setQuality(RenderManager::quality() - 5); this->refresh(); }, 0.5f);
        rui::label(m_root, std::to_string(RenderManager::quality()), right - 46.f, y, 0.42f, rui::TEXT, false);
        addPill("+", right - 14.f, y, 28.f, 24.f, rui::SOFT, rui::TEXT,
                [this]() { RenderManager::setQuality(RenderManager::quality() + 5); this->refresh(); }, 0.5f);
        y -= step;
        divider(y + step / 2);

        auto sw = [&](char const* title, bool value, std::function<void(bool)> onChange) {
            rui::label(m_root, title, left, y, 0.4f, rui::TEXT, true, 200.f);
            addItem(rui::toggle(value), right - 18.f, y, [this, onChange, value]() { onChange(!value); this->refresh(); });
            y -= step;
            divider(y + step / 2);
        };
        sw("Rekam suara (musik level)", RenderManager::audio(), [](bool v) { RenderManager::setAudio(v); });
        sw("Stop saat level selesai", RenderManager::autoStop(), [](bool v) { RenderManager::setAutoStop(v); });
        sw("Tampilkan hitbox", RenderManager::hitboxes(), [](bool v) { RenderManager::setHitboxes(v); });
        sw("Indikator REC di layar", RenderManager::showHud(), [](bool v) { RenderManager::setShowHud(v); });

        addPill("Tentang mod", PW / 2, 24.f, 130.f, 26.f, rui::SOFT, rui::DIM, [this]() { this->go(Page::Info); }, 0.34f);
    }

    // ---------- layar video ----------
    void buildVideos(CCDrawNode* shapes) {
        this->header("Video");
        auto list = RenderManager::files();
        if (list.empty()) {
            rui::label(m_root, "Belum ada video", PW / 2, PH / 2 + 20.f, 0.5f, rui::TEXT, false);
            rui::body(m_root, "Rekam dulu lewat tombol merah", PW / 2, PH / 2 - 2.f, rui::DIM, 0.8f, false);
        }
        size_t shown = (std::min)(list.size(), static_cast<size_t>(4));
        for (size_t i = 0; i < shown; i++) {
            float y = PH - 84.f - static_cast<float>(i) * 48.f;
            rui::fillRect(shapes, 28.f, y - 24.f, PW - 56.f, 1.f, ccColor3B{236, 240, 250});
            auto path = list[i];
            rui::label(m_root, path.filename().string(), 28.f, y + 8.f, 0.36f, rui::TEXT, true, 190.f);
            rui::body(m_root, RenderManager::sizeText(path), 28.f, y - 10.f, rui::GREEN, 0.8f);
            addPill("BUKA", PW - 128.f, y, 56.f, 26.f, rui::BLUE, rui::WHITE, [path]() {
                if (!geode::utils::file::openFolder(path)) geode::utils::file::openFolder(RenderManager::dir());
            }, 0.34f);
            addPill("HAPUS", PW - 62.f, y, 62.f, 26.f, rui::RECSOFT, rui::REC, [this, path]() {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                this->refresh();
            }, 0.34f);
        }
        addPill("Buka folder video", PW / 2, 26.f, 170.f, 28.f, rui::GREEN, rui::WHITE, []() {
            if (!geode::utils::file::openFolder(RenderManager::dir())) {
                Notification::create("Tidak bisa membuka folder", NotificationIcon::Warning)->show();
            }
        }, 0.36f);
    }

    // ---------- layar tentang ----------
    void buildInfo() {
        this->header("Tentang");
        char const* lines[] = {
            "Rendering Utils v2.5",
            "",
            "Rekam gameplay Geometry Dash jadi",
            "video MP4 langsung dari dalam game.",
            "",
            "- Ketuk tombol merah: level diulang dari",
            "  awal dan direkam otomatis.",
            "- Level selesai = video langsung tersimpan.",
            "- Musik level ikut direkam.",
            "- HP lag? Game melambat otomatis supaya",
            "  video tetap mulus.",
            "- Berhenti di tengah: Pause > Render."
        };
        float y = PH - 70.f;
        bool first = true;
        for (auto const* ln : lines) {
            if (ln[0] != '\0') {
                if (first) rui::label(m_root, ln, 28.f, y, 0.42f, rui::BLUE);
                else rui::body(m_root, ln, 28.f, y, rui::TEXT, 0.78f, true, PW - 56.f);
            }
            first = false;
            y -= 17.f;
        }
    }

    void onRecordButton() {
        auto& r = RenderManager::get();
        if (r.rendering) {
            r.stop(true);
            this->refresh();
            return;
        }
        auto* pl = PlayLayer::get();
        if (!pl) {
            Notification::create("Buka dulu sebuah level", NotificationIcon::Warning)->show();
            return;
        }
        if (!r.start(pl->m_level->m_levelName)) {
            Notification::create("Gagal memulai render (cek penyimpanan)", NotificationIcon::Error)->show();
            return;
        }
        Notification::create("Render dimulai", NotificationIcon::Success)->show();
        auto* pause = m_pause;
        this->onClose(nullptr);
        if (pause) pause->onRestartFull(nullptr);
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
