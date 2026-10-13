#pragma once
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>
#include <functional>
#include "Render.hpp"

// ============================================================
// UI Rendering Utils - gaya sama seperti Karot Utils:
//  - tombol kecil "RU" di pause menu -> panel gelap di sisi kiri
//  - Opsi & Video = popup GD biasa dengan tombol hijau/biru/merah
// ============================================================
namespace kbui {

    // Tombol buatan sendiri: ukuran & teks seimbang.
    // bg: "GJ_button_01.png" (hijau), "GJ_button_02.png" (biru), "GJ_button_04.png" (abu), "GJ_button_06.png" (merah)
    inline CCNode* buttonNode(char const* text, float w, float h, char const* bg, float textScale = 0.55f) {
        auto node = CCNode::create();
        node->setContentSize({w, h});
        auto spr = NineSlice::create(bg, CCRect{0.f, 0.f, 40.f, 40.f}, NineSlice::Insets{12.f, 12.f, 12.f, 12.f});
        if (spr) {
            spr->setContentSize({w, h});
            spr->setPosition({w / 2, h / 2});
            node->addChild(spr, 0);
        }
        auto label = CCLabelBMFont::create(text, "goldFont.fnt");
        label->limitLabelWidth(w - 12.f, textScale, 0.2f);
        label->setPosition({w / 2, h / 2 + 1.f});
        node->addChild(label, 1);
        return node;
    }

    // Pil/kapsul gelap
    inline NineSlice* pill(float w, float h, ccColor3B color, GLubyte opacity) {
        auto bg = NineSlice::create("square02_001.png");
        bg->setContentSize({w, h});
        bg->setColor(color);
        bg->setOpacity(opacity);
        return bg;
    }

    // Lingkaran penuh (polygon, karena drawDot di game ini tampil sebagai kotak)
    inline void circle(CCDrawNode* d, float cx, float cy, float r, ccColor4F c) {
        constexpr int N = 28;
        CCPoint p[N];
        for (int i = 0; i < N; i++) {
            float a = static_cast<float>(i) * 6.2831853f / static_cast<float>(N);
            p[i] = CCPoint{cx + r * std::cos(a), cy + r * std::sin(a)};
        }
        d->drawPolygon(p, N, c, 0.f, c);
    }

    inline CCLabelBMFont* text(CCNode* parent, std::string const& s, float x, float y, float maxW, float scale,
                               char const* font = "bigFont.fnt", bool left = true, ccColor3B color = {255, 255, 255}) {
        auto l = CCLabelBMFont::create(s.c_str(), font);
        l->limitLabelWidth(maxW, scale, 0.15f);
        l->setAnchorPoint({left ? 0.f : 0.5f, 0.5f});
        l->setColor(color);
        l->setPosition({x, y});
        parent->addChild(l, 2);
        return l;
    }
}

// ============================================================
// Popup Opsi
// ============================================================
class RenderOptionsPopup : public geode::Popup {
protected:
    CCNode* m_page = nullptr;

    bool init() {
        if (!Popup::init(340.f, 232.f)) return false;
        this->setTitle("Opsi Render");
        this->build();
        return true;
    }

    void refresh() {
        geode::Loader::get()->queueInMainThread([self = geode::Ref<RenderOptionsPopup>(this)]() { self->build(); });
    }

    void build() {
        if (m_page) m_page->removeFromParent();
        m_page = CCNode::create();
        m_mainLayer->addChild(m_page, 5);
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_page->addChild(menu, 5);

        float W = m_size.width;

        auto chipRow = [&](char const* title, float y, std::vector<std::pair<std::string, int>> const& items,
                           int current, std::function<void(int)> set) {
            kbui::text(m_page, title, 18.f, y, 70.f, 0.5f);
            float w = 50.f, gap = 4.f;
            float x = W - 16.f - static_cast<float>(items.size()) * (w + gap) + gap;
            for (auto const& it : items) {
                bool sel = (it.second == current);
                int val = it.second;
                auto b = CCMenuItemExt::createSpriteExtra(
                    kbui::buttonNode(it.first.c_str(), w, 26.f, sel ? "GJ_button_01.png" : "GJ_button_04.png", 0.5f),
                    [this, set, val](CCObject*) { set(val); this->refresh(); });
                b->setPosition({x + w / 2, y});
                menu->addChild(b);
                x += w + gap;
            }
        };
        chipRow("Resolusi", 176.f, {{"360p", 360}, {"480p", 480}, {"720p", 720}, {"1080p", 1080}},
                RenderManager::heightP(), [](int v) { RenderManager::setHeightP(v); });
        chipRow("FPS", 144.f, {{"24", 24}, {"30", 30}, {"60", 60}},
                RenderManager::fps(), [](int v) { RenderManager::setFps(v); });

        // kualitas
        {
            float y = 112.f;
            kbui::text(m_page, "Kualitas", 18.f, y, 70.f, 0.5f);
            auto minus = CCMenuItemExt::createSpriteExtra(kbui::buttonNode("-", 34.f, 26.f, "GJ_button_04.png", 0.7f),
                [this](CCObject*) { RenderManager::setQuality(RenderManager::quality() - 5); this->refresh(); });
            minus->setPosition({W - 108.f, y});
            menu->addChild(minus);
            kbui::text(m_page, std::to_string(RenderManager::quality()), W - 66.f, y, 40.f, 0.55f, "bigFont.fnt", false);
            auto plus = CCMenuItemExt::createSpriteExtra(kbui::buttonNode("+", 34.f, 26.f, "GJ_button_04.png", 0.7f),
                [this](CCObject*) { RenderManager::setQuality(RenderManager::quality() + 5); this->refresh(); });
            plus->setPosition({W - 33.f, y});
            menu->addChild(plus);
        }

        auto sw = [&](char const* title, float x, float y, bool value, std::function<void(bool)> onChange) {
            auto t = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [onChange](CCMenuItemToggler* tg) {
                onChange(!tg->isToggled());   // callback jalan sebelum state berubah
            });
            t->toggle(value);
            t->setPosition({x, y});
            menu->addChild(t);
            kbui::text(m_page, title, x + 18.f, y, 112.f, 0.42f);
        };
        sw("Rekam suara", 30.f, 74.f, RenderManager::audio(), [](bool v) { RenderManager::setAudio(v); });
        sw("Stop otomatis", 182.f, 74.f, RenderManager::autoStop(), [](bool v) { RenderManager::setAutoStop(v); });
        sw("Tampil hitbox", 30.f, 40.f, RenderManager::hitboxes(), [](bool v) { RenderManager::setHitboxes(v); });
        sw("Indikator REC", 182.f, 40.f, RenderManager::showHud(), [](bool v) { RenderManager::setShowHud(v); });
    }

public:
    static RenderOptionsPopup* create() {
        auto ret = new RenderOptionsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ============================================================
// Popup Video hasil render
// ============================================================
class RenderVideosPopup : public geode::Popup {
protected:
    CCNode* m_page = nullptr;

    bool init() {
        if (!Popup::init(350.f, 232.f)) return false;
        this->setTitle("Video Hasil");
        this->build();
        return true;
    }

    void refresh() {
        geode::Loader::get()->queueInMainThread([self = geode::Ref<RenderVideosPopup>(this)]() { self->build(); });
    }

    void build() {
        if (m_page) m_page->removeFromParent();
        m_page = CCNode::create();
        m_mainLayer->addChild(m_page, 5);
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_page->addChild(menu, 5);

        float W = m_size.width;
        auto list = RenderManager::files();
        if (list.empty()) {
            kbui::text(m_page, "Belum ada video.", W / 2, 130.f, 280.f, 0.5f, "bigFont.fnt", false);
            kbui::text(m_page, "Tekan Render di panel pause untuk mulai.", W / 2, 106.f, 300.f, 0.6f, "chatFont.fnt", false, {165, 178, 215});
        }
        size_t shown = (std::min)(list.size(), static_cast<size_t>(4));
        float rowH = 36.f;
        for (size_t i = 0; i < shown; i++) {
            float y = 178.f - static_cast<float>(i) * (rowH + 4.f);
            auto bg = kbui::pill(W - 30.f, rowH, {0, 0, 0}, 120);
            bg->setPosition({W / 2, y});
            m_page->addChild(bg, 1);

            auto path = list[i];
            kbui::text(m_page, path.filename().string(), 24.f, y + 7.f, 150.f, 0.42f);
            kbui::text(m_page, RenderManager::sizeText(path), 24.f, y - 9.f, 100.f, 0.62f, "chatFont.fnt", true, {150, 200, 255});

            auto open = CCMenuItemExt::createSpriteExtra(kbui::buttonNode("Buka", 56.f, 26.f, "GJ_button_01.png", 0.5f),
                [path](CCObject*) {
                    if (!geode::utils::file::openFolder(path)) geode::utils::file::openFolder(RenderManager::dir());
                });
            open->setPosition({W - 108.f, y});
            menu->addChild(open);
            auto del = CCMenuItemExt::createSpriteExtra(kbui::buttonNode("Hapus", 56.f, 26.f, "GJ_button_06.png", 0.5f),
                [this, path](CCObject*) {
                    std::error_code ec;
                    std::filesystem::remove(path, ec);
                    this->refresh();
                });
            del->setPosition({W - 48.f, y});
            menu->addChild(del);
        }

        auto folder = CCMenuItemExt::createSpriteExtra(kbui::buttonNode("Buka Folder", 150.f, 28.f, "GJ_button_02.png", 0.55f),
            [](CCObject*) {
                if (!geode::utils::file::openFolder(RenderManager::dir())) {
                    Notification::create("Tidak bisa membuka folder", NotificationIcon::Warning)->show();
                }
            });
        folder->setPosition({W / 2, 22.f});
        menu->addChild(folder);
    }

public:
    static RenderVideosPopup* create() {
        auto ret = new RenderVideosPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ============================================================
// Panel di PAUSE MENU (sisi kiri). Awalnya hanya tombol kecil "RU".
// ============================================================
class RenderPanel : public CCNode {
protected:
    static constexpr float PW = 148.f;
    static constexpr float PH = 190.f;

    PauseLayer* m_pause = nullptr;
    CCNode* m_body = nullptr;
    bool m_open = false;

    bool setup(PauseLayer* pause) {
        if (!CCNode::init()) return false;
        m_pause = pause;
        auto& r = RenderManager::get();
        bool rec = r.rendering;

        this->setContentSize({PW, PH + 30.f});

        m_body = CCNode::create();
        m_body->ignoreAnchorPointForPosition(false);
        m_body->setContentSize({PW, PH});
        m_body->setAnchorPoint({0.f, 1.f});
        m_body->setPosition({0.f, PH});
        m_body->setVisible(false);
        m_body->setScale(0.01f);
        this->addChild(m_body, 1);

        auto bg = kbui::pill(PW, PH, {14, 18, 40}, 235);
        bg->setPosition({PW / 2, PH / 2});
        m_body->addChild(bg, 0);

        auto title = CCLabelBMFont::create("Rendering Utils", "goldFont.fnt");
        title->limitLabelWidth(PW - 20.f, 0.55f, 0.3f);
        title->setPosition({PW / 2, PH - 18.f});
        m_body->addChild(title, 2);

        auto line = CCLayerColor::create({255, 205, 90, 170}, PW - 28.f, 1.5f);
        line->setPosition({14.f, PH - 33.f});
        m_body->addChild(line, 2);

        // status
        ccColor3B pillColor = rec ? ccColor3B{150, 30, 40} : ccColor3B{55, 60, 90};
        auto st = kbui::pill(PW - 26.f, 19.f, pillColor, 230);
        st->setPosition({PW / 2, PH - 51.f});
        m_body->addChild(st, 1);
        std::string statusText = "IDLE";
        if (rec) {
            int secs = r.frames / (std::max)(1, RenderManager::fps());
            char buf[48];
            std::snprintf(buf, sizeof(buf), "REC %02d:%02d", secs / 60, secs % 60);
            statusText = buf;
        }
        kbui::text(m_body, statusText, PW / 2, PH - 51.f, PW - 36.f, 0.36f, "bigFont.fnt", false);

        std::string infoText;
        ccColor3B infoColor = {165, 178, 215};
        if (rec) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%.1fx %s", r.speedX, r.speedX >= 0.9 ? "lancar" : (r.speedX >= 0.6 ? "agak lag" : "lag"));
            infoText = buf;
            infoColor = r.speedX >= 0.9 ? ccColor3B{110, 235, 150} : (r.speedX >= 0.6 ? ccColor3B{255, 214, 90} : ccColor3B{255, 120, 120});
        } else {
            infoText = std::to_string(RenderManager::heightP()) + "p  " + std::to_string(RenderManager::fps()) + " fps";
        }
        kbui::text(m_body, infoText, PW / 2, PH - 70.f, PW - 24.f, 0.65f, "chatFont.fnt", false, infoColor);

        // tombol
        const float bw = 124.f, bh = 28.f;
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        menu->setContentSize({PW, PH});
        m_body->addChild(menu, 5);

        auto recBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode(rec ? "Stop" : "Render", bw, bh, rec ? "GJ_button_04.png" : "GJ_button_06.png", 0.62f),
            [this](CCObject*) { this->onRecord(); });
        recBtn->setPosition({PW / 2, 100.f});
        menu->addChild(recBtn);

        const float hw = 62.f;
        auto vidBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode("Video", hw, bh, "GJ_button_01.png", 0.6f),
            [](CCObject*) { RenderVideosPopup::create()->show(); });
        vidBtn->setPosition({PW / 2 - hw / 2 - 2.f, 64.f});
        menu->addChild(vidBtn);

        auto optBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode("Opsi", hw, bh, "GJ_button_02.png", 0.6f),
            [](CCObject*) { RenderOptionsPopup::create()->show(); });
        optBtn->setPosition({PW / 2 + hw / 2 + 2.f, 64.f});
        menu->addChild(optBtn);

        auto sep = CCLayerColor::create({255, 255, 255, 45}, PW - 28.f, 1.f);
        sep->setPosition({14.f, 42.f});
        m_body->addChild(sep, 2);

        auto snd = CCMenuItemExt::createTogglerWithStandardSprites(0.55f, [](CCMenuItemToggler* t) {
            RenderManager::setAudio(!t->isToggled());
        });
        snd->toggle(RenderManager::audio());
        snd->setPosition({26.f, 22.f});
        menu->addChild(snd);
        kbui::text(m_body, "Rekam suara", 44.f, 22.f, PW - 56.f, 0.36f);

        // tombol kecil "RU"
        auto toggleMenu = CCMenu::create();
        toggleMenu->setPosition({0, 0});
        toggleMenu->setContentSize({PW, PH + 30.f});
        this->addChild(toggleMenu, 10);

        auto toggleBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode("RU", 40.f, 26.f, rec ? "GJ_button_06.png" : "GJ_button_02.png", 0.6f),
            [this](CCObject*) { this->togglePanel(); });
        toggleBtn->setPosition({20.f, PH + 14.f});
        toggleMenu->addChild(toggleBtn);
        return true;
    }

    void togglePanel() {
        m_open = !m_open;
        m_body->stopAllActions();
        if (m_open) {
            m_body->setVisible(true);
            m_body->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)));
        } else {
            m_body->runAction(CCSequence::create(CCScaleTo::create(0.12f, 0.01f), CCHide::create(), nullptr));
        }
    }

    void onRecord() {
        auto& r = RenderManager::get();
        if (r.rendering) {
            r.stop(true);
            m_pause->onResume(nullptr);
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
        m_pause->onRestartFull(nullptr);
    }

public:
    static RenderPanel* create(PauseLayer* pause) {
        auto ret = new RenderPanel();
        if (ret->setup(pause)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
