#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "MacroManager.hpp"
#include "Popups.hpp"
#include "Settings.hpp"
#include "Control.hpp"

using namespace geode::prelude;

static double kbClamp(double v, double lo, double hi) {
    if (hi < lo) return lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

// ============================================================
// Hook input & loop game:
//  - record / playback macro
//  - Auto Clicker
//  - Auto Slope Wave
//  - Speed hack (mulus) & Show Hitboxes
// ============================================================
class $modify(KBBaseGameLayer, GJBaseGameLayer) {

    // Pilih player dari nilai flag handleButton
    PlayerObject* kbPlayer(bool flag) {
        auto& m = MacroManager::get();
        return (flag == (m.p1Flag != 0)) ? m_player1 : m_player2;
    }

    void kbRecordClick(bool down, int button, bool flag) {
        auto& m = MacroManager::get();
        int frame = static_cast<int>(m_gameState.m_currentProgress);
        auto* pl = this->kbPlayer(flag);
        CCPoint p = pl ? pl->getPosition() : CCPoint{0.f, 0.f};
        double yv = pl ? pl->m_yVelocity : 0.0;
        m.clicks.push_back(Click{frame, button, down, flag, false, p.x, p.y, yv});
    }

    // Kirim tombol utama ke game (player 1, dan player 2 kalau mode dual).
    // Kalau sedang Recording, klik ikut direkam.
    void kbSend(bool down) {
        auto& m = MacroManager::get();
        bool recording = m.state == MacroManager::State::Recording && PlayLayer::get();
        bool f1 = (m.p1Flag != 0);

        if (recording) this->kbRecordClick(down, 1, f1);
        m.gameDown = down;
        GJBaseGameLayer::handleButton(down, 1, f1);

        if (m_gameState.m_isDualMode) {
            if (recording) this->kbRecordClick(down, 1, !f1);
            GJBaseGameLayer::handleButton(down, 1, !f1);
        }
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto& m = MacroManager::get();
        bool inLevel = PlayLayer::get() != nullptr;

        // Pelajari nilai flag untuk "player 1" dari sentuhan asli (hanya saat tidak dual)
        if (inLevel && button == 1 && !m.injecting && !m_gameState.m_isDualMode) {
            m.p1Flag = isPlayer1 ? 1 : 0;
        }
        bool isMain = inLevel && button == 1 && (isPlayer1 == (m.p1Flag != 0));

        // Selalu ingat posisi tombol layar yang sebenarnya (untuk sinkron saat bot berhenti)
        if (isMain) m.realDown = down;

        // Saat playback, sentuhan pemain diabaikan (hanya klik dari macro)
        if (m.state == MacroManager::State::Playing && !m.injecting) return;

        // Auto Clicker / Auto Slope: bot yang menentukan tombol (lihat kbDrive)
        if (isMain && m.drivesInput() && !m.injecting) return;

        // Saat rekam, simpan setiap klik beserta nomor step & posisi player
        if (m.state == MacroManager::State::Recording && !m.injecting && inLevel) {
            this->kbRecordClick(down, button, isPlayer1);
        }

        if (isMain) m.gameDown = down;
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    // ----------------------------------------------------------
    // Auto Slope Wave.
    // Hasil: -1 = tidak ada slope relevan (kontrol balik ke pemain), 0 = lepas, 1 = tahan
    // ----------------------------------------------------------
    int kbSlope(int tick) {
        auto& m = MacroManager::get();
        auto* p = m_player1;
        m.hudText = "SLOPE: bukan wave";
        if (!p || !p->m_isDart) {          // hanya aktif di mode wave
            m.lastAction = -1;
            return -1;
        }

        // Cache semua objek slope di level
        size_t objCount = m_objects ? static_cast<size_t>(m_objects->count()) : 0;
        if (m.slopeOwner != this || m.slopeObjCount != objCount) {
            m.slopes.clear();
            m.slopeOwner = this;
            m.slopeObjCount = objCount;
            if (m_objects) {
                for (auto* obj : CCArrayExt<GameObject*>(m_objects)) {
                    if (obj && obj->m_objectType == GameObjectType::Slope) m.slopes.push_back(obj);
                }
            }
        }
        m.hudText = "SLOPE: wave, slope di level=" + std::to_string(m.slopes.size());
        if (m.slopes.empty()) {
            m.lastAction = -1;
            return -1;
        }

        CCPoint pos = p->getPosition();
        bool up = p->m_isUpsideDown;
        bool mini = p->m_vehicleSize < 0.9f;

        // ---- Kalibrasi otomatis: seberapa cepat wave naik/turun per satuan x ----
        if (!m.calibInit || up != m.lastUpside || mini != m.lastMini) {
            double r = mini ? 2.0 : 1.0;
            m.kHold = up ? -r : r;
            m.kRel = -m.kHold;
            m.calibInit = true;
            m.lastUpside = up;
            m.lastMini = mini;
            m.lastAction = -1;
        }
        if (m.lastAction >= 0 && tick != m.lastFrame) {
            float dx = pos.x - m.lastX;
            if (dx > 0.01f) {
                double k = (pos.y - m.lastY) / dx;
                if (std::fabs(k) > 0.3 && std::fabs(k) < 4.0) {
                    double& ref = (m.lastAction == 1) ? m.kHold : m.kRel;
                    ref = ref * 0.8 + k * 0.2;
                }
            }
            if (std::fabs(m.kHold - m.kRel) < 0.5) {     // kalibrasi rusak -> ulang
                m.calibInit = false;
            }
        }
        m.lastX = pos.x;
        m.lastY = pos.y;
        m.lastFrame = tick;

        // ---- Cari slope terdekat di bawah & di atas wave ----
        const double L = static_cast<double>(MacroManager::slopeLook());
        const double ahead = L * 0.35;      // titik sampling di depan wave
        const double window = 110.0;        // jarak vertikal maksimum slope yang dianggap relevan
        const double margin = mini ? 5.0 : 8.0;

        bool haveBelow = false, haveAbove = false;
        double belowY = 0, belowTan = 0, aboveY = 0, aboveTan = 0;
        int nearCount = 0;

        for (auto* s : m.slopes) {
            if (!s || s->m_isNoTouch || s->m_isPassable) continue;
            auto const& r = s->getObjectRect();
            double minX = r.getMinX(), maxX = r.getMaxX();
            if (maxX < pos.x - 2.0 || minX > pos.x + L) continue;

            double xe = kbClamp(pos.x + ahead, minX, maxX);
            double ys = s->slopeYPos(static_cast<float>(xe));
            if (!std::isfinite(ys)) continue;
            ys = kbClamp(ys, static_cast<double>(r.getMinY()) - 2.0, static_cast<double>(r.getMaxY()) + 2.0);

            double xa = kbClamp(xe - 4.0, minX, maxX);
            double xb = kbClamp(xe + 4.0, minX, maxX);
            double tn = 0.0;
            if ((xb - xa) > 0.5) {
                tn = (static_cast<double>(s->slopeYPos(static_cast<float>(xb))) -
                      static_cast<double>(s->slopeYPos(static_cast<float>(xa)))) / (xb - xa);
            }
            if (!std::isfinite(tn)) tn = 0.0;

            double yNow, tanUse;
            if (pos.x >= minX) {
                // Wave sudah di atas slope: proyeksikan garis slope ke posisi x sekarang
                yNow = ys - tn * (xe - pos.x);
                tanUse = tn;
            } else {
                // Slope belum sampai: arahkan ke titik masuknya, tanpa kemiringan
                yNow = ys;
                tanUse = 0.0;
            }

            if (std::fabs(yNow - pos.y) > window) continue;
            nearCount++;

            if (yNow <= pos.y) {
                if (!haveBelow || yNow > belowY) { haveBelow = true; belowY = yNow; belowTan = tanUse; }
            } else {
                if (!haveAbove || yNow < aboveY) { haveAbove = true; aboveY = yNow; aboveTan = tanUse; }
            }
        }

        double target, tanT;
        if (haveBelow && haveAbove) {
            target = ((belowY + margin) + (aboveY - margin)) / 2.0;   // tengah-tengah lorong
            tanT = (belowTan + aboveTan) / 2.0;
        } else if (haveBelow) {
            target = belowY + margin;
            tanT = belowTan;
        } else if (haveAbove) {
            target = aboveY - margin;
            tanT = aboveTan;
        } else {
            m.lastAction = -1;
            m.hudText = "SLOPE: wave, tidak ada slope dekat";
            return -1;
        }

        // ---- Hitung rasio tahan/lepas (PWM) supaya rata-rata mengikuti slope ----
        double dHold = (tanT - m.kRel) / (m.kHold - m.kRel);          // feed-forward dari kemiringan
        double sgn = (m.kHold > m.kRel) ? 1.0 : -1.0;
        dHold += sgn * (target - pos.y) / 12.0;                         // koreksi posisi
        dHold = kbClamp(dHold, 0.0, 1.0);

        int T = static_cast<int>(std::lround(MacroManager::TPS / static_cast<double>(MacroManager::slopeCps())));
        if (T < 2) T = 2;
        int holdSteps = static_cast<int>(std::lround(dHold * T));
        bool desired = (tick % T) < holdSteps;

        m.lastAction = desired ? 1 : 0;
        m.hudText = "SLOPE: AKTIF (" + std::to_string(nearCount) + " dekat) " + (desired ? "tahan" : "lepas") +
                    "  duty " + std::to_string(static_cast<int>(dHold * 100.0)) + "%";
        return desired ? 1 : 0;
    }

    // ----------------------------------------------------------
    // Tentukan tombol yang seharusnya ditekan tiap step
    // (Auto Slope > Auto Clicker > sentuhan pemain)
    // Juga menyinkronkan tombol saat bot berhenti, supaya tidak ada tombol "nyangkut".
    // ----------------------------------------------------------
    void kbDrive(int frame) {
        auto& m = MacroManager::get();

        // Penghitung step: pakai counter game; kalau counter itu ternyata tidak bergerak, pakai hitungan sendiri
        m.callCount++;
        if (frame != m.seenFrame) { m.seenFrame = frame; m.frameMoves++; }
        int tick = (m.callCount > 12 && m.frameMoves < 3) ? m.callCount : frame;

        bool desired = m.realDown;
        bool controlled = false;
        m.hudText.clear();

        if (m.slopeOn) {
            int d = this->kbSlope(tick);
            if (d >= 0) {
                desired = (d == 1);
                controlled = true;
                m.tainted = true;
            }
        }
        if (!controlled && m.acOn && (!m.acHoldOnly || m.realDown)) {
            int P = static_cast<int>(std::lround(MacroManager::TPS / static_cast<double>(MacroManager::acCps())));
            if (P < 2) P = 2;
            int holdSteps = (P + 1) / 2;
            desired = (tick % P) < holdSteps;
            controlled = true;
            m.tainted = true;
            m.hudText = "AUTO CLICKER: " + std::to_string(MacroManager::acCps()) + " cps";
        }

        if (desired != m.gameDown) this->kbSend(desired);
    }

    // ---- Show Hitboxes ----
    void kbHitboxes() {
        bool on = MacroManager::hitboxes();
        static bool wasOn = false;
        if (on) {
            if (!m_isDebugDrawEnabled) m_isDebugDrawEnabled = true;
            if (m_debugDrawNode && !m_debugDrawNode->isVisible()) m_debugDrawNode->setVisible(true);
        } else if (wasOn) {
            m_isDebugDrawEnabled = false;
            if (m_debugDrawNode) {
                m_debugDrawNode->clear();
                m_debugDrawNode->setVisible(false);
            }
        }
        wasOn = on;
    }

    void update(float dt) {
        // Speed hack: kecepatan berubah pelan-pelan supaya mulus
        if (PlayLayer::get()) MacroManager::get().tickSpeed();
        GJBaseGameLayer::update(dt);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& m = MacroManager::get();

        if (PlayLayer::get() && m_player1) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);

            this->kbHitboxes();

            // ---- RECORD: simpan snapshot posisi tiap beberapa step ----
            if (m.state == MacroManager::State::Recording && !m.injecting) {
                bool already = !m.clicks.empty() && m.clicks.back().snap && m.clicks.back().frame == frame;
                if (frame % MacroManager::snapInterval() == 0 && !already) {
                    CCPoint p = m_player1->getPosition();
                    m.clicks.push_back(Click{frame, -1, false, true, true, p.x, p.y, m_player1->m_yVelocity});
                }
            }

            if (m.state == MacroManager::State::Playing) {
                // ---- PLAY: tekan tombol sesuai macro + koreksi posisi ----
                m.injecting = true;
                while (m.index < m.clicks.size() && m.clicks[m.index].frame <= frame) {
                    auto const c = m.clicks[m.index++];
                    auto* pl = this->kbPlayer(c.p1);

                    // Koreksi posisi hanya kalau step-nya persis sama dengan saat rekam
                    if (pl && c.frame == frame && (c.px != 0.f || c.py != 0.f)) {
                        pl->setPosition(CCPoint{c.px, c.py});
                        pl->m_yVelocity = c.yv;
                    }

                    if (!c.snap) {
                        if (c.button == 1 && c.p1 == (m.p1Flag != 0)) m.gameDown = c.down;
                        GJBaseGameLayer::handleButton(c.down, c.button, c.p1);
                    }
                }
                m.injecting = false;
                m.hudText = "MACRO PLAY";

                // Macro habis -> bot berhenti, kontrol kembali ke pemain
                if (m.index >= m.clicks.size()) {
                    m.stop();
                    Notification::create("Macro selesai, kontrol kembali ke kamu", NotificationIcon::Info)->show();
                }
            } else {
                // ---- Auto Clicker / Auto Slope / sinkron tombol ----
                this->kbDrive(frame);
            }

            this->kbUpdateHud(frame);
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }

    // ---- HUD kecil untuk melihat apa yang sedang dilakukan bot ----
    void kbUpdateHud(int frame) {
        auto& m = MacroManager::get();
        auto* pl = PlayLayer::get();
        if (!pl || !pl->m_uiLayer) return;
        auto* node = pl->m_uiLayer->getChildByTag(7771);
        auto* label = node ? static_cast<CCLabelBMFont*>(node) : nullptr;
        if (!label) return;

        if (!MacroManager::hud()) {
            if (label->isVisible()) label->setVisible(false);
            return;
        }
        std::string text = m.hudText;
        if (std::fabs(m.speedCur - 1.f) > 0.001f) {
            if (!text.empty()) text += "   ";
            text += "SPEED " + std::to_string(static_cast<int>(std::lround(m.speedCur * 100.f))) + "%";
        }
        if (text.empty()) {
            if (label->isVisible()) label->setVisible(false);
            return;
        }
        (void)frame;
        if (!label->isVisible()) label->setVisible(true);
        if (text != std::string(label->getString())) label->setString(text.c_str());
    }
};

// ============================================================
// SAFE MODE: progress tidak naik / tidak tersimpan saat bot aktif
// ============================================================
class $modify(KBGameLevel, GJGameLevel) {
    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        auto& m = MacroManager::get();
        if (m.botActive() && MacroManager::safe()) {
            return;   // jangan simpan persen apa pun
        }
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

// ============================================================
// Hook PlayLayer: mati/reset di practice mode, autosave, Safe Mode
// ============================================================
class $modify(KBPlayLayer, PlayLayer) {
    struct Fields {
        bool restartPending = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        // Masuk level baru: bot berhenti, status bersih
        auto& m = MacroManager::get();
        m.stop();
        m.tainted = false;
        m.realDown = false;
        m.gameDown = false;
        m.clearSlopeCache();
        m.resetSpeed();
        m.hudText.clear();

        // Label HUD kecil di kiri bawah (hanya muncul saat ada yang aktif)
        if (m_uiLayer) {
            auto label = CCLabelBMFont::create("", "chatFont.fnt");
            label->setAnchorPoint({0.f, 0.f});
            label->setScale(0.55f);
            label->setOpacity(190);
            label->setColor({255, 230, 140});
            label->setPosition({6.f, 4.f});
            label->setVisible(false);
            label->setTag(7771);
            m_uiLayer->addChild(label, 150);
        }
        return true;
    }

    void resetLevelFromStart() {
        PlayLayer::resetLevelFromStart();
        // Attempt baru dari awal: status "tercemar bot" dibersihkan
        MacroManager::get().tainted = false;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->restartPending = false;

        auto& m = MacroManager::get();
        m.clearSlopeCache();
        m.realDown = false;
        m.gameDown = false;
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

        bool botActive = m.botActive();
        m.resetSpeed();

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
        m.tainted = false;
        m.clearSlopeCache();
        m.resetSpeed();
        PlayLayer::onQuit();
    }
};

// ============================================================
// Panel Karot Utils di dalam pause menu (sisi kiri)
// ============================================================
class $modify(KBPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        MacroManager::get().resetSpeed();   // pause menu selalu kecepatan normal

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
