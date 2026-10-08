#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "MacroManager.hpp"
#include "Popups.hpp"

using namespace geode::prelude;

// ============================================================
// Hook input & loop game: record klik dan putar ulang macro
// ============================================================
class $modify(KBBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& m = MacroManager::get();

        // Saat playback, input manual pemain diabaikan (hanya klik dari bot)
        if (m.state == MacroManager::State::Playing && !m.injecting) return;

        // Saat rekam, simpan setiap klik beserta nomor frame-nya
        if (m.state == MacroManager::State::Recording && !m.injecting && PlayLayer::get()) {
            m.clicks.push_back({m_gameState.m_currentProgress, button, down, isPlayer1});
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt) {
        auto& m = MacroManager::get();

        if (m.state == MacroManager::State::Playing && PlayLayer::get()) {
            int frame = m_gameState.m_currentProgress;
            m.injecting = true;
            while (m.index < m.clicks.size() && m.clicks[m.index].frame <= frame) {
                auto const& c = m.clicks[m.index++];
                GJBaseGameLayer::handleButton(c.down, c.button, c.p1);
            }
            m.injecting = false;
        }

        GJBaseGameLayer::processCommands(dt);
    }
};

// ============================================================
// Hook PlayLayer: handle mati/reset di practice mode & keluar level
// ============================================================
class $modify(KBPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();

        auto& m = MacroManager::get();
        int frame = m_gameState.m_currentProgress;

        if (m.state == MacroManager::State::Recording) {
            // Mati -> balik ke checkpoint: buang klik setelah checkpoint, lanjut rekam dari situ
            m.truncateFrom(frame);
        } else if (m.state == MacroManager::State::Playing) {
            m.seek(frame);
        }
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        auto& m = MacroManager::get();
        if (m.state == MacroManager::State::Playing) m.stop();
    }

    void onQuit() {
        auto& m = MacroManager::get();
        // Jangan sampai rekaman hilang kalau keluar level sebelum sempat disimpan
        if (m.state == MacroManager::State::Recording && !m.clicks.empty()) {
            m.save("autosave");
        }
        m.stop();
        PlayLayer::onQuit();
    }
};

// ============================================================
// Tombol Karim Bot di pause menu
// ============================================================
class $modify(KBPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("karim-bot-menu"_spr);
        menu->setPosition({0, 0});

        auto& m = MacroManager::get();
        bool recording = m.state == MacroManager::State::Recording;
        bool playing = m.state == MacroManager::State::Playing;

        float x = 60.f;
        float y = win.height / 2 + 45.f;

        auto title = CCLabelBMFont::create("Karim Bot", "goldFont.fnt");
        title->setScale(0.6f);
        title->setPosition({x, y + 32.f});
        this->addChild(title);

        // --- Tombol Record / Stop ---
        auto recBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create(recording ? "Stop Rec" : "Record", "goldFont.fnt",
                                 recording ? "GJ_button_06.png" : "GJ_button_01.png", 0.7f),
            [this](CCObject*) {
                auto& m = MacroManager::get();
                if (m.state == MacroManager::State::Recording) {
                    m.stop();
                    if (!m.clicks.empty()) {
                        SaveMacroPopup::create()->show();
                    } else {
                        Notification::create("Tidak ada klik yang terekam", NotificationIcon::Warning)->show();
                    }
                } else {
                    m.stop();
                    m.clicks.clear();
                    m.loadedName.clear();
                    m.state = MacroManager::State::Recording;
                    Notification::create("Rekam aktif! Restart level / lanjut main", NotificationIcon::Success)->show();
                }
                this->onResume(nullptr);
            }
        );
        recBtn->setPosition({x, y});
        menu->addChild(recBtn);

        // --- Tombol Play / Stop ---
        auto playBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create(playing ? "Stop Play" : "Play", "goldFont.fnt",
                                 playing ? "GJ_button_06.png" : "GJ_button_02.png", 0.7f),
            [this](CCObject*) {
                auto& m = MacroManager::get();
                if (m.state == MacroManager::State::Playing) {
                    m.stop();
                    Notification::create("Playback dihentikan", NotificationIcon::Info)->show();
                } else if (m.clicks.empty()) {
                    Notification::create("Belum ada macro. Record atau Load dulu!", NotificationIcon::Warning)->show();
                    return;
                } else {
                    m.stop();
                    m.seek(0);
                    m.state = MacroManager::State::Playing;
                    Notification::create("Playback aktif! Restart level dari awal", NotificationIcon::Success)->show();
                }
                this->onResume(nullptr);
            }
        );
        playBtn->setPosition({x, y - 38.f});
        menu->addChild(playBtn);

        // --- Tombol Macros (tempat penyimpanan + Load) ---
        auto macroBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Load", "goldFont.fnt", "GJ_button_04.png", 0.7f),
            [](CCObject*) {
                MacroListPopup::create()->show();
            }
        );
        macroBtn->setPosition({x, y - 76.f});
        menu->addChild(macroBtn);

        // --- Status ---
        std::string status = recording ? "REC..." : playing ? "PLAY..." : "Idle";
        if (!m.loadedName.empty()) status += " [" + m.loadedName + "]";
        auto statusLabel = CCLabelBMFont::create(status.c_str(), "bigFont.fnt");
        statusLabel->limitLabelWidth(100.f, 0.4f, 0.2f);
        statusLabel->setPosition({x, y - 108.f});
        this->addChild(statusLabel);

        this->addChild(menu);
    }
};
