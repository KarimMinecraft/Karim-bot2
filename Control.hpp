#pragma once
#include "MacroManager.hpp"
#include "Popups.hpp"
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>

// ============================================================
// Karot Utils di dalam PAUSE MENU (sisi kiri layar)
// Awalnya hanya 1 tombol kecil "KU". Ditekan -> panel terbuka:
// status, Record, Play, Load, dan Safe Mode
// ============================================================
class KBPanel : public CCNode {
protected:
    static constexpr float PW = 132.f;   // lebar panel
    static constexpr float PH = 216.f;   // tinggi panel

    PauseLayer* m_pause = nullptr;
    CCNode* m_body = nullptr;   // isi panel (disembunyikan sampai tombol kecil ditekan)
    bool m_open = false;

    bool setup(PauseLayer* pause) {
        if (!CCNode::init()) return false;
        m_pause = pause;

        auto& m = MacroManager::get();
        bool rec = m.state == MacroManager::State::Recording;
        bool play = m.state == MacroManager::State::Playing;

        // root: tombol kecil di atas, panel di bawahnya
        this->setContentSize({PW, PH + 30.f});

        m_body = CCNode::create();
        m_body->ignoreAnchorPointForPosition(false);
        m_body->setContentSize({PW, PH});
        m_body->setAnchorPoint({0.f, 1.f});
        m_body->setPosition({0.f, PH});
        m_body->setVisible(false);
        m_body->setScale(0.01f);
        this->addChild(m_body, 1);

        // ---- latar panel ----
        auto bg = NineSlice::create("square02_001.png");
        bg->setContentSize({PW, PH});
        bg->setColor({14, 18, 40});
        bg->setOpacity(235);
        bg->setPosition({PW / 2, PH / 2});
        m_body->addChild(bg, 0);

        // ---- judul + garis aksen emas ----
        auto title = CCLabelBMFont::create("Karot Utils", "goldFont.fnt");
        title->limitLabelWidth(PW - 20.f, 0.6f, 0.3f);
        title->setPosition({PW / 2, PH - 18.f});
        m_body->addChild(title, 2);

        auto line = CCLayerColor::create({255, 205, 90, 170}, PW - 28.f, 1.5f);
        line->setPosition({14.f, PH - 33.f});
        m_body->addChild(line, 2);

        // ---- pil status (warna berubah sesuai status) ----
        ccColor3B pillColor = rec ? ccColor3B{150, 30, 40} : play ? ccColor3B{25, 120, 55} : ccColor3B{55, 60, 90};
        auto pill = kbui::pill(PW - 26.f, 19.f, pillColor, 230);
        pill->setPosition({PW / 2, PH - 51.f});
        m_body->addChild(pill, 1);

        // jumlah klik macro (yang direkam / yang di-load) ikut tampil di status
        std::string countText = "  " + std::to_string(m.liveClickCount()) + " klik";
        std::string statusText = rec ? "REC" + countText
                                : play ? "PLAY" + countText
                                : (m.clicks.empty() ? std::string("IDLE") : "IDLE" + countText);
        auto status = CCLabelBMFont::create(statusText.c_str(), "bigFont.fnt");
        status->limitLabelWidth(PW - 36.f, 0.34f, 0.2f);
        status->setPosition({PW / 2, PH - 51.f});
        m_body->addChild(status, 2);

        // ---- nama macro / level ----
        std::string infoText = rec ? m.levelName : (m.loadedName.empty() ? "belum ada macro" : m.loadedName);
        auto info = CCLabelBMFont::create(infoText.c_str(), "chatFont.fnt");
        info->limitLabelWidth(PW - 24.f, 0.6f, 0.3f);
        info->setColor({165, 178, 215});
        info->setPosition({PW / 2, PH - 70.f});
        m_body->addChild(info, 2);

        // ---- tombol (semua ukuran sama, rapi ditengah) ----
        const float bw = 108.f, bh = 28.f;
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        menu->setContentSize({PW, PH});
        m_body->addChild(menu, 5);

        auto recBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode(rec ? "Stop" : "Record", bw, bh, rec ? "GJ_button_04.png" : "GJ_button_06.png", 0.62f),
            [this](CCObject*) { this->onRecord(); }
        );
        recBtn->setPosition({PW / 2, 126.f});
        menu->addChild(recBtn);

        auto playBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode(play ? "Stop" : "Play", bw, bh, play ? "GJ_button_04.png" : "GJ_button_01.png", 0.62f),
            [this](CCObject*) { this->onPlay(); }
        );
        playBtn->setPosition({PW / 2, 92.f});
        menu->addChild(playBtn);

        auto loadBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode("Load", bw, bh, "GJ_button_02.png", 0.62f),
            [](CCObject*) { MacroListPopup::create()->show(); }
        );
        loadBtn->setPosition({PW / 2, 58.f});
        menu->addChild(loadBtn);

        // ---- pemisah + Safe Mode ----
        auto sep = CCLayerColor::create({255, 255, 255, 45}, PW - 28.f, 1.f);
        sep->setPosition({14.f, 38.f});
        m_body->addChild(sep, 2);

        auto safe = CCMenuItemExt::createTogglerWithStandardSprites(0.55f, [](CCMenuItemToggler* t) {
            bool on = !t->isToggled();   // callback jalan sebelum state berubah
            MacroManager::setSafe(on);
            if (on) {
                Notification::create("Safe Mode ON: progress tidak naik saat bot jalan", NotificationIcon::Success)->show();
            } else {
                Notification::create("Safe Mode OFF: progress BISA terkirim, hati-hati!", NotificationIcon::Warning)->show();
            }
        });
        safe->toggle(MacroManager::safe());
        safe->setPosition({26.f, 19.f});
        menu->addChild(safe);

        auto safeLabel = CCLabelBMFont::create("Safe Mode", "bigFont.fnt");
        safeLabel->limitLabelWidth(PW - 56.f, 0.36f, 0.2f);
        safeLabel->setAnchorPoint({0.f, 0.5f});
        safeLabel->setPosition({44.f, 19.f});
        m_body->addChild(safeLabel, 2);

        // ---- tombol kecil "KU" (selalu terlihat) ----
        auto toggleMenu = CCMenu::create();
        toggleMenu->setPosition({0, 0});
        toggleMenu->setContentSize({PW, PH + 30.f});
        this->addChild(toggleMenu, 10);

        auto toggleBtn = CCMenuItemExt::createSpriteExtra(
            kbui::buttonNode("KU", 40.f, 26.f, "GJ_button_02.png", 0.6f),
            [this](CCObject*) { this->togglePanel(); }
        );
        toggleBtn->setPosition({20.f, PH + 14.f});
        toggleMenu->addChild(toggleBtn);

        return true;
    }

    // ---- buka / tutup panel ----
    void togglePanel() {
        m_open = !m_open;
        m_body->stopAllActions();
        if (m_open) {
            m_body->setVisible(true);
            m_body->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)));
        } else {
            m_body->runAction(CCSequence::create(
                CCScaleTo::create(0.12f, 0.01f),
                CCHide::create(),
                nullptr
            ));
        }
    }

    // ---- aksi tombol ----
    void onRecord() {
        auto pl = PlayLayer::get();
        if (!pl) return;
        auto& m = MacroManager::get();

        // Sedang merekam -> stop & simpan otomatis pakai nama level
        if (m.state == MacroManager::State::Recording) {
            auto name = m.finishRecording();
            if (!name.empty()) {
                Notification::create("Macro disimpan: " + name, NotificationIcon::Success)->show();
            } else {
                Notification::create("Tidak ada klik yang terekam", NotificationIcon::Warning)->show();
            }
            m_pause->onResume(nullptr);
            return;
        }

        m.stop();
        m.clicks.clear();
        m.loadedName.clear();
        m.levelName = pl->m_level->m_levelName;

        // Langsung hidupkan practice mode, lalu mulai rekam dari awal level
        if (!pl->m_isPracticeMode) pl->togglePracticeMode(true);
        m.state = MacroManager::State::Recording;

        Notification::create("Rekam dimulai (practice mode aktif)", NotificationIcon::Success)->show();
        m_pause->onRestartFull(nullptr);
    }

    void onPlay() {
        auto pl = PlayLayer::get();
        if (!pl) return;
        auto& m = MacroManager::get();

        if (m.state == MacroManager::State::Playing) {
            m.stop();
            Notification::create("Playback dihentikan", NotificationIcon::Info)->show();
            m_pause->onResume(nullptr);
            return;
        }
        if (m.state == MacroManager::State::Recording) {
            m.finishRecording();
        }
        if (m.clicks.empty()) {
            Notification::create("Belum ada macro. Record atau Load dulu!", NotificationIcon::Warning)->show();
            return;
        }

        m.stop();
        m.seek(0);
        m.state = MacroManager::State::Playing;

        Notification::create("Playback dimulai", NotificationIcon::Success)->show();
        m_pause->onRestartFull(nullptr);
    }

public:
    static KBPanel* create(PauseLayer* pause) {
        auto ret = new KBPanel();
        if (ret->setup(pause)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
