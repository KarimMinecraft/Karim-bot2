#pragma once
#include "MacroManager.hpp"
#include "Popups.hpp"
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>

// ============================================================
// Tombol kecil Karim Bot di layar level.
// Ditekan -> muncul panel: Record, Play, Load, dan Safe Mode.
// ============================================================
class KBControl : public CCNode {
protected:
    static constexpr float PW = 150.f;   // lebar panel
    static constexpr float PH = 186.f;   // tinggi panel

    CCNode* m_panel = nullptr;
    CCMenu* m_menu = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_info = nullptr;
    CCLabelBMFont* m_chip = nullptr;
    bool m_open = false;
    int m_sig = -1;

    bool init() override {
        if (!CCNode::init()) return false;

        // ---- tombol kecil (selalu terlihat) ----
        auto toggleMenu = CCMenu::create();
        toggleMenu->setPosition({0, 0});
        auto kb = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("KB", 34, true, "goldFont.fnt", "GJ_button_01.png", 24.f, 0.6f),
            [this](CCObject*) { this->togglePanel(); }
        );
        toggleMenu->addChild(kb);
        this->addChild(toggleMenu, 2);

        // ---- chip status kecil di bawah tombol (REC / PLAY) ----
        m_chip = CCLabelBMFont::create("", "bigFont.fnt");
        m_chip->setScale(0.35f);
        m_chip->setPosition({0.f, -20.f});
        this->addChild(m_chip, 2);

        // ---- panel ----
        m_panel = CCNode::create();
        m_panel->ignoreAnchorPointForPosition(false);
        m_panel->setContentSize({PW, PH});
        m_panel->setAnchorPoint({0.f, 1.f});
        m_panel->setPosition({24.f, 14.f});
        m_panel->setVisible(false);
        m_panel->setScale(0.01f);
        this->addChild(m_panel, 1);

        auto bg = NineSlice::create("square02_001.png");
        bg->setContentSize({PW, PH});
        bg->setColor({10, 12, 28});
        bg->setOpacity(215);
        bg->setPosition({PW / 2, PH / 2});
        m_panel->addChild(bg, 0);

        // garis aksen tipis di bawah judul
        auto line = CCLayerColor::create({90, 160, 255, 160}, PW - 36.f, 1.5f);
        line->setPosition({18.f, PH - 33.f});
        m_panel->addChild(line, 1);

        auto title = CCLabelBMFont::create("Karim Bot", "goldFont.fnt");
        title->setScale(0.55f);
        title->setPosition({PW / 2, PH - 16.f});
        m_panel->addChild(title, 1);

        m_status = CCLabelBMFont::create("Idle", "bigFont.fnt");
        m_status->setScale(0.36f);
        m_status->setPosition({PW / 2, PH - 47.f});
        m_panel->addChild(m_status, 1);

        m_info = CCLabelBMFont::create("", "chatFont.fnt");
        m_info->setScale(0.6f);
        m_info->setColor({160, 175, 210});
        m_info->setPosition({PW / 2, PH - 62.f});
        m_panel->addChild(m_info, 1);

        this->rebuild();
        this->schedule(schedule_selector(KBControl::tick), 0.15f);
        return true;
    }

    // Susun ulang isi menu (tombol berubah sesuai status: Record <-> Stop, Play <-> Stop)
    void rebuild() {
        if (m_menu) {
            m_menu->removeFromParent();
            m_menu = nullptr;
        }
        auto& m = MacroManager::get();
        bool rec = m.state == MacroManager::State::Recording;
        bool play = m.state == MacroManager::State::Playing;

        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_menu->setContentSize({PW, PH});
        m_panel->addChild(m_menu, 5);

        auto recBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create(rec ? "Stop" : "Record", 108, true, "goldFont.fnt",
                                 rec ? "GJ_button_04.png" : "GJ_button_06.png", 26.f, 0.7f),
            [this](CCObject*) { this->onRecord(); }
        );
        recBtn->setPosition({PW / 2, 106.f});
        m_menu->addChild(recBtn);

        auto playBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create(play ? "Stop" : "Play", 108, true, "goldFont.fnt",
                                 play ? "GJ_button_04.png" : "GJ_button_01.png", 26.f, 0.7f),
            [this](CCObject*) { this->onPlay(); }
        );
        playBtn->setPosition({PW / 2, 74.f});
        m_menu->addChild(playBtn);

        auto loadBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Load", 108, true, "goldFont.fnt", "GJ_button_02.png", 26.f, 0.7f),
            [](CCObject*) { MacroListPopup::create()->show(); }
        );
        loadBtn->setPosition({PW / 2, 42.f});
        m_menu->addChild(loadBtn);

        // ---- Safe Mode ----
        auto safe = CCMenuItemExt::createTogglerWithStandardSprites(0.5f, [](CCMenuItemToggler* t) {
            bool on = !t->isToggled();   // callback jalan sebelum state berubah
            MacroManager::setSafe(on);
            if (on) {
                Notification::create("Safe Mode ON: progress tidak naik saat bot jalan", NotificationIcon::Success)->show();
            } else {
                Notification::create("Safe Mode OFF: progress BISA terkirim, hati-hati!", NotificationIcon::Warning)->show();
            }
        });
        safe->toggle(MacroManager::safe());
        safe->setPosition({24.f, 15.f});
        m_menu->addChild(safe);

        auto safeLabel = CCLabelBMFont::create("Safe Mode", "bigFont.fnt");
        safeLabel->setScale(0.34f);
        safeLabel->setAnchorPoint({0.f, 0.5f});
        safeLabel->setPosition({40.f, 15.f});
        m_panel->addChild(safeLabel, 1);
        safeLabel->setTag(7001);
    }

    // Update status tiap 0.15 detik
    void tick(float) {
        auto& m = MacroManager::get();
        int sig = static_cast<int>(m.state) * 2 + (m.clicks.empty() ? 0 : 1);
        if (sig != m_sig) {
            m_sig = sig;
            // buang label Safe Mode lama sebelum menyusun ulang
            if (auto old = m_panel->getChildByTag(7001)) old->removeFromParent();
            this->rebuild();
        }

        switch (m.state) {
            case MacroManager::State::Recording:
                m_status->setString(("REC  " + std::to_string(m.liveClickCount()) + " klik").c_str());
                m_status->setColor({255, 90, 90});
                m_info->setString(m.levelName.c_str());
                this->setChip("REC", {255, 70, 70});
                break;
            case MacroManager::State::Playing:
                m_status->setString("PLAY");
                m_status->setColor({110, 255, 140});
                m_info->setString(m.loadedName.c_str());
                this->setChip("PLAY", {90, 255, 130});
                break;
            default:
                m_status->setString("Idle");
                m_status->setColor({200, 205, 225});
                m_info->setString(m.loadedName.empty() ? "belum ada macro" : m.loadedName.c_str());
                this->setChip("", {255, 255, 255});
                break;
        }
    }

    void setChip(char const* text, ccColor3B color) {
        if (std::string(m_chip->getString()) == text) return;
        m_chip->setString(text);
        m_chip->setColor(color);
        m_chip->stopAllActions();
        m_chip->setOpacity(255);
        if (text[0] != '\0') {
            m_chip->runAction(CCRepeatForever::create(CCSequence::create(
                CCFadeTo::create(0.5f, 90),
                CCFadeTo::create(0.5f, 255),
                nullptr
            )));
        }
    }

    // ---- animasi buka/tutup panel ----
    void togglePanel() {
        m_open = !m_open;
        m_panel->stopAllActions();
        if (m_open) {
            m_panel->setVisible(true);
            m_panel->runAction(CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)));
        } else {
            m_panel->runAction(CCSequence::create(
                CCScaleTo::create(0.12f, 0.01f),
                CCHide::create(),
                nullptr
            ));
        }
    }

    void closePanel() {
        if (m_open) this->togglePanel();
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
            return;
        }

        m.stop();
        m.clicks.clear();
        m.loadedName.clear();
        m.levelName = pl->m_level->m_levelName;

        // Langsung hidupkan practice mode, lalu mulai rekam dari awal level
        if (!pl->m_isPracticeMode) pl->togglePracticeMode(true);
        m.state = MacroManager::State::Recording;
        pl->resetLevelFromStart();

        Notification::create("Rekam dimulai (practice mode aktif)", NotificationIcon::Success)->show();
        this->closePanel();
    }

    void onPlay() {
        auto pl = PlayLayer::get();
        if (!pl) return;
        auto& m = MacroManager::get();

        if (m.state == MacroManager::State::Playing) {
            m.stop();
            Notification::create("Playback dihentikan", NotificationIcon::Info)->show();
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
        pl->resetLevelFromStart();

        Notification::create("Playback dimulai", NotificationIcon::Success)->show();
        this->closePanel();
    }

public:
    static KBControl* create() {
        auto ret = new KBControl();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
