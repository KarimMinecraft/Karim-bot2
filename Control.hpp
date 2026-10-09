#pragma once
#include "MacroManager.hpp"
#include "Popups.hpp"
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>

// ============================================================
// Panel Karot Utils di dalam PAUSE MENU (sisi kiri layar)
// Isi: status, Record, Play, Load, dan Safe Mode
// ============================================================
class KBPanel : public CCNode {
protected:
    static constexpr float PW = 132.f;   // lebar panel
    static constexpr float PH = 216.f;   // tinggi panel

    PauseLayer* m_pause = nullptr;

    bool setup(PauseLayer* pause) {
        if (!CCNode::init()) return false;
        m_pause = pause;

        auto& m = MacroManager::get();
        bool rec = m.state == MacroManager::State::Recording;
        bool play = m.state == MacroManager::State::Playing;

        this->setContentSize({PW, PH});

        // ---- latar panel ----
        auto bg = NineSlice::create("square02_001.png");
        bg->setContentSize({PW, PH});
        bg->setColor({14, 18, 40});
        bg->setOpacity(235);
        bg->setPosition({PW / 2, PH / 2});
        this->addChild(bg, 0);

        // ---- judul + garis aksen emas ----
        auto title = CCLabelBMFont::create("Karot Utils", "goldFont.fnt");
        title->limitLabelWidth(PW - 20.f, 0.6f, 0.3f);
        title->setPosition({PW / 2, PH - 18.f});
        this->addChild(title, 2);

        auto line = CCLayerColor::create({255, 205, 90, 170}, PW - 28.f, 1.5f);
        line->setPosition({14.f, PH - 33.f});
        this->addChild(line, 2);

        // ---- pil status (warna berubah sesuai status) ----
        ccColor3B pillColor = rec ? ccColor3B{150, 30, 40} : play ? ccColor3B{25, 120, 55} : ccColor3B{55, 60, 90};
        auto pill = kbui::pill(PW - 26.f, 19.f, pillColor, 230);
        pill->setPosition({PW / 2, PH - 51.f});
        this->addChild(pill, 1);

        std::string statusText = rec ? "REC  " + std::to_string(m.liveClickCount()) + " klik"
                                : play ? "PLAY"
                                : "IDLE";
        auto status = CCLabelBMFont::create(statusText.c_str(), "bigFont.fnt");
        status->limitLabelWidth(PW - 36.f, 0.34f, 0.2f);
        status->setPosition({PW / 2, PH - 51.f});
        this->addChild(status, 2);

        // ---- nama macro / level ----
        std::string infoText = rec ? m.levelName : (m.loadedName.empty() ? "belum ada macro" : m.loadedName);
        auto info = CCLabelBMFont::create(infoText.c_str(), "chatFont.fnt");
        info->limitLabelWidth(PW - 24.f, 0.6f, 0.3f);
        info->setColor({165, 178, 215});
        info->setPosition({PW / 2, PH - 70.f});
        this->addChild(info, 2);

        // ---- tombol (semua ukuran sama, rapi ditengah) ----
        const float bw = 108.f, bh = 28.f;
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        menu->setContentSize({PW, PH});
        this->addChild(menu, 5);

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
        this->addChild(sep, 2);

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
        this->addChild(safeLabel, 2);

        return true;
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
