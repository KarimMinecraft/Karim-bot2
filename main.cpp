#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "MacroManager.hpp"
#include "Popups.hpp"
#include "Control.hpp"

using namespace geode::prelude;

// Interval (dalam step fisika) untuk snapshot posisi player.
// Snapshot dipakai mengoreksi drift saat playback.
static constexpr int SNAPSHOT_INTERVAL = 10;

// ============================================================
// Hook input & loop game: record klik dan putar ulang macro
// ============================================================
class $modify(KBBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& m = MacroManager::get();

        // Saat playback, input manual pemain diabaikan (hanya klik dari bot)
        if (m.state == MacroManager::State::Playing && !m.injecting) return;

        // Saat rekam, simpan setiap klik beserta nomor step & posisi player
        if (m.state == MacroManager::State::Recording && !m.injecting && PlayLayer::get()) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);
            auto* pl = isPlayer1 ? m_player1 : m_player2;
            CCPoint p = pl ? pl->getPosition() : CCPoint{0.f, 0.f};
            double yv = pl ? pl->m_yVelocity : 0.0;
            m.clicks.push_back(Click{frame, button, down, isPlayer1, false, p.x, p.y, yv});
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& m = MacroManager::get();

        if (PlayLayer::get()) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);

            // ---- RECORD: simpan snapshot posisi tiap beberapa step ----
            if (m.state == MacroManager::State::Recording && !m.injecting && m_player1) {
                bool already = !m.clicks.empty() && m.clicks.back().snap && m.clicks.back().frame == frame;
                if (frame % SNAPSHOT_INTERVAL == 0 && !already) {
                    CCPoint p = m_player1->getPosition();
                    m.clicks.push_back(Click{frame, -1, false, true, true, p.x, p.y, m_player1->m_yVelocity});
                }
            }

            // ---- PLAY: tekan tombol sesuai macro + koreksi posisi ----
            if (m.state == MacroManager::State::Playing) {
                m.injecting = true;
                while (m.index < m.clicks.size() && m.clicks[m.index].frame <= frame) {
                    auto const c = m.clicks[m.index++];
                    auto* pl = c.p1 ? m_player1 : m_player2;

                    // Koreksi posisi hanya kalau step-nya persis sama dengan saat rekam
                    if (pl && c.frame == frame && (c.px != 0.f || c.py != 0.f)) {
                        pl->setPosition(CCPoint{c.px, c.py});
                        pl->m_yVelocity = c.yv;
                    }

                    if (!c.snap) {
                        GJBaseGameLayer::handleButton(c.down, c.button, c.p1);
                    }
                }
                m.injecting = false;
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

// ============================================================
// SAFE MODE: progress tidak naik / tidak tersimpan saat bot aktif
// ============================================================
class $modify(KBGameLevel, GJGameLevel) {
    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        auto& m = MacroManager::get();
        if (m.state != MacroManager::State::Idle && MacroManager::safe()) {
            return;   // jangan simpan persen apa pun
        }
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

// ============================================================
// Hook PlayLayer: tombol UI, mati/reset di practice mode, autosave
// ============================================================
class $modify(KBPlayLayer, PlayLayer) {
    struct Fields {
        bool restartPending = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        // Masuk level baru: pastikan bot dalam keadaan berhenti
        MacroManager::get().stop();
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->restartPending = false;

        auto& m = MacroManager::get();
        int frame = static_cast<int>(m_gameState.m_currentProgress);

        if (m.state == MacroManager::State::Recording) {
            // Mati -> balik ke checkpoint: buang entri setelah checkpoint, lanjut rekam dari situ
            m.truncateFrom(frame);
        } else if (m.state == MacroManager::State::Playing) {
            m.seek(frame);
        }
    }

    void kbRestart() {
        m_fields->restartPending = false;
        this->resetLevelFromStart();
    }

    void levelComplete() {
        auto& m = MacroManager::get();

        // Sudah menunggu restart (Safe Mode) -> jangan proses ulang
        if (m_fields->restartPending) return;

        bool botActive = m.state != MacroManager::State::Idle;

        // AUTOSAVE: level selesai saat merekam -> simpan otomatis pakai nama level
        if (m.state == MacroManager::State::Recording) {
            auto name = m.finishRecording();
            if (!name.empty()) {
                Notification::create("Macro disimpan otomatis: " + name, NotificationIcon::Success)->show();
            }
        } else if (m.state == MacroManager::State::Playing) {
            m.stop();
        }

        // SAFE MODE: jangan kirim penyelesaian level / progress, ulang dari awal
        if (botActive && MacroManager::safe()) {
            m_fields->restartPending = true;
            Notification::create("Safe Mode: penyelesaian level tidak dikirim", NotificationIcon::Info)->show();
            this->runAction(CCSequence::create(
                CCDelayTime::create(0.5f),
                CCCallFunc::create(this, callfunc_selector(KBPlayLayer::kbRestart)),
                nullptr
            ));
            return;
        }

        PlayLayer::levelComplete();
    }

    void onQuit() {
        auto& m = MacroManager::get();
        // Jangan sampai rekaman hilang kalau keluar level sebelum selesai
        if (m.state == MacroManager::State::Recording && !m.clicks.empty()) {
            m.finishRecording("_partial");
        }
        m.stop();
        PlayLayer::onQuit();
    }
};

// ============================================================
// Panel Karot Utils di dalam pause menu (sisi kiri)
// ============================================================
class $modify(KBPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto panel = KBPanel::create(this);
        if (!panel) return;

        float x = 14.f;
        float y = 52.f;
        panel->setPosition({x - 40.f, y});
        this->addChild(panel, 100);

        // animasi masuk dari kiri
        panel->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.3f, {x, y})));
    }
};
