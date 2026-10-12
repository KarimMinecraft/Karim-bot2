#pragma once
#include <Geode/utils/file.hpp>
#include <functional>
#include "Render.hpp"

// ============================================================
// RENDER STUDIO v3 - satu layar ala aplikasi kamera:
//  kiri  : tombol rekam bulat besar + indikator kecepatan
//  kanan : pengaturan (pilihan cepat & saklar) + video terakhir
// Semua digambar sendiri (tanpa tekstur game) dengan warna terang.
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
    static constexpr float PW = 460.f;
    static constexpr float PH = 280.f;

    PauseLayer* m_pause = nullptr;
    CCNode* m_root = nullptr;
    CCMenu* m_menu = nullptr;
    bool m_info = false;

    bool init(PauseLayer* pause) {
        if (!Popup::init(PW, PH)) return false;
        m_pause = pause;
        this->setOpacity(70);
        if (m_bgSprite) m_bgSprite->setVisible(false);

        auto d = CCDrawNode::create();
        rui::fillRound(d, 0.f, 0.f, PW, PH, 20.f, rui::BLUE);
        rui::fillRound(d, 3.f, 3.f, PW - 6.f, PH - 6.f, 18.f, rui::BG);
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

    // ---------- gambar ulang seluruh isi ----------
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

        auto& r = RenderManager::get();

        // --- judul ---
        rui::label(m_root, "Rendering Utils", 24.f, PH - 22.f, 0.52f, rui::TEXT);
        rui::body(m_root, "Rekam gameplay jadi video MP4  -  v2.4", 24.f, PH - 40.f, rui::DIM, 0.7f);
        addItem(rui::pill("i", 26.f, 26.f, rui::BLUE, rui::WHITE, 0.5f), PW - 30.f, PH - 28.f,
                [this]() { m_info = !m_info; this->refresh(); });

        // --- kartu kiri: rekam ---
        rui::fillRound(shapes, 14.f, 14.f, 160.f, 208.f, 16.f, rui::WHITE);
        {
            auto node = CCNode::create();
            node->setContentSize({108.f, 108.f});
            auto d = CCDrawNode::create();
            d->drawDot({54.f, 54.f}, 52.f, rui::f4(rui::RECSOFT));
            d->drawDot({54.f, 54.f}, 42.f, rui::f4(rui::REC));
            if (r.rendering) rui::fillRound(d, 54.f - 14.f, 54.f - 14.f, 28.f, 28.f, 6.f, rui::WHITE);
            else d->drawDot({54.f, 54.f}, 15.f, rui::f4(rui::WHITE));
            node->addChild(d);
            addItem(node, 94.f, 156.f, [this]() { this->onRecordButton(); });
        }
        rui::label(m_root, r.rendering ? "STOP" : "MULAI", 94.f, 96.f, 0.5f, rui::REC, false);

        if (r.rendering) {
            int secs = r.frames / (std::max)(1, RenderManager::fps());
            char buf[64];
            std::snprintf(buf, sizeof(buf), "REC %02d:%02d", secs / 60, secs % 60);
            rui::body(m_root, buf, 94.f, 76.f, rui::TEXT, 0.8f, false);
        } else {
            rui::body(m_root, "Siap merekam", 94.f, 76.f, rui::DIM, 0.8f, false);
        }

        // indikator kecepatan (10 kotak)
        {
            double sp = r.rendering ? r.speedX : 0.0;
            int filled = static_cast<int>(std::lround((std::min)(1.0, (std::max)(0.0, sp)) * 10.0));
            float x0 = 94.f - 59.f;
            for (int i = 0; i < 10; i++) {
                rui::fillRound(shapes, x0 + i * 12.f, 46.f, 10.f, 8.f, 3.f,
                               i < filled ? speedColor(sp) : ccColor3B{222, 229, 246});
            }
            if (r.rendering) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.1fx  %s", sp, speedText(sp));
                rui::label(m_root, buf, 94.f, 28.f, 0.4f, speedColor(sp), false);
            } else {
                rui::body(m_root, "Kecepatan render", 94.f, 28.f, rui::DIM, 0.7f, false);
            }
        }

        if (m_info) {
            this->buildInfo(shapes);
            return;
        }

        // --- kartu kanan atas: pengaturan ---
        rui::fillRound(shapes, 184.f, 82.f, 262.f, 140.f, 16.f, rui::WHITE);
        float rx = 438.f;   // tepi kanan kontrol

        auto segRow = [&](char const* title, float y, std::vector<std::pair<std::string, int>> const& items,
                          int current, std::function<void(int)> set) {
            rui::label(m_root, title, 198.f, y, 0.38f, rui::TEXT, true, 58.f);
            float w = 44.f, gap = 3.f;
            float x = rx - static_cast<float>(items.size()) * (w + gap) + gap;
            for (auto const& it : items) {
                bool sel = (it.second == current);
                int val = it.second;
                addPill(it.first, x + w / 2, y, w, 22.f, sel ? rui::BLUE : rui::SOFT, sel ? rui::WHITE : rui::TEXT,
                        [this, set, val]() { set(val); this->refresh(); }, 0.36f);
                x += w + gap;
            }
        };
        segRow("Resolusi", 202.f, {{"360p", 360}, {"480p", 480}, {"720p", 720}, {"1080p", 1080}},
               RenderManager::heightP(), [](int v) { RenderManager::setHeightP(v); });
        segRow("FPS", 174.f, {{"24", 24}, {"30", 30}, {"60", 60}},
               RenderManager::fps(), [](int v) { RenderManager::setFps(v); });

        // kualitas: - nilai +
        {
            float y = 146.f;
            rui::label(m_root, "Kualitas", 198.f, y, 0.38f, rui::TEXT, true, 58.f);
            addPill("-", rx - 100.f, y, 28.f, 22.f, rui::SOFT, rui::TEXT,
                    [this]() { RenderManager::setQuality(RenderManager::quality() - 5); this->refresh(); }, 0.5f);
            rui::fillRound(shapes, rx - 82.f, y - 11.f, 54.f, 22.f, 11.f, rui::SOFT);
            rui::label(m_root, std::to_string(RenderManager::quality()), rx - 55.f, y, 0.4f, rui::TEXT, false);
            addPill("+", rx - 14.f, y, 28.f, 22.f, rui::SOFT, rui::TEXT,
                    [this]() { RenderManager::setQuality(RenderManager::quality() + 5); this->refresh(); }, 0.5f);
        }

        // saklar 2 kolom
        auto sw = [&](char const* title, float cx, float y, bool value, std::function<void(bool)> onChange) {
            rui::fillRound(shapes, cx - 61.f, y - 12.f, 122.f, 24.f, 12.f, ccColor3B{240, 244, 255});
            rui::label(m_root, title, cx - 53.f, y, 0.33f, rui::TEXT, true, 56.f);
            addItem(rui::toggle(value), cx + 38.f, y, [this, onChange, value]() { onChange(!value); this->refresh(); });
        };
        sw("Suara", 252.f, 120.f, RenderManager::audio(), [](bool v) { RenderManager::setAudio(v); });
        sw("Auto stop", 378.f, 120.f, RenderManager::autoStop(), [](bool v) { RenderManager::setAutoStop(v); });
        sw("Hitbox", 252.f, 94.f, RenderManager::hitboxes(), [](bool v) { RenderManager::setHitboxes(v); });
        sw("Indikator", 378.f, 94.f, RenderManager::showHud(), [](bool v) { RenderManager::setShowHud(v); });

        // --- kartu kanan bawah: video terakhir ---
        rui::fillRound(shapes, 184.f, 14.f, 262.f, 60.f, 16.f, rui::WHITE);
        auto list = RenderManager::files();
        if (list.empty()) {
            rui::body(m_root, "Belum ada video. Tekan tombol merah untuk mulai.", 198.f, 54.f, rui::DIM, 0.7f, true, 236.f);
        } else {
            auto path = list[0];
            rui::label(m_root, path.filename().string(), 198.f, 60.f, 0.34f, rui::TEXT, true, 190.f);
            rui::body(m_root, RenderManager::sizeText(path), 400.f, 60.f, rui::GREEN, 0.75f);
        }
        float by = 32.f;
        addPill("BUKA", 244.f, by, 70.f, 24.f, rui::BLUE, rui::WHITE, [list]() {
            if (list.empty()) {
                Notification::create("Belum ada video", NotificationIcon::Info)->show();
                return;
            }
            if (!geode::utils::file::openFolder(list[0])) geode::utils::file::openFolder(RenderManager::dir());
        }, 0.4f);
        addPill("FOLDER", 322.f, by, 78.f, 24.f, rui::GREEN, rui::WHITE, []() {
            if (!geode::utils::file::openFolder(RenderManager::dir())) {
                Notification::create("Tidak bisa membuka folder", NotificationIcon::Warning)->show();
            }
        }, 0.4f);
        addPill("HAPUS", 402.f, by, 70.f, 24.f, rui::RECSOFT, rui::REC, [this, list]() {
            if (list.empty()) return;
            std::error_code ec;
            std::filesystem::remove(list[0], ec);
            this->refresh();
        }, 0.4f);
    }

    void buildInfo(CCDrawNode* shapes) {
        rui::fillRound(shapes, 184.f, 14.f, 262.f, 208.f, 16.f, rui::WHITE);
        rui::label(m_root, "TENTANG MOD", 200.f, 204.f, 0.46f, rui::BLUE);
        char const* lines[] = {
            "Rekam gameplay Geometry Dash jadi video",
            "MP4 langsung dari dalam game.",
            "",
            "- Tekan tombol merah: level diulang dari",
            "  awal dan direkam otomatis.",
            "- Level selesai = video langsung tersimpan.",
            "- Musik level ikut direkam.",
            "- HP lag? Game melambat otomatis supaya",
            "  video tetap mulus (lihat indikator).",
            "- Berhenti di tengah: Pause > Render."
        };
        float y = 182.f;
        for (auto const* ln : lines) {
            if (ln[0] != '\0') rui::body(m_root, ln, 200.f, y, rui::TEXT, 0.7f, true, 236.f);
            y -= 14.f;
        }
        addPill("TUTUP", 315.f, 32.f, 120.f, 26.f, rui::BLUE, rui::WHITE,
                [this]() { m_info = false; this->refresh(); }, 0.45f);
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
