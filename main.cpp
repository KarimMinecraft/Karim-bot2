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

// ============================================================
// Hook input & loop game:
//  - record / playback macro
//  - Auto Clicker
//  - Auto Slope Wave
// ============================================================
class $modify(KBBaseGameLayer, GJBaseGameLayer) {

    // Kirim tombol 1 (player 1) ke game. Kalau sedang Recording, klik ikut direkam.
    void kbSend(bool down) {
        auto& m = MacroManager::get();
        if (m.state == MacroManager::State::Recording && PlayLayer::get()) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);
            CCPoint p = m_player1 ? m_player1->getPosition() : CCPoint{0.f, 0.f};
            double yv = m_player1 ? m_player1->m_yVelocity : 0.0;
            m.clicks.push_back(Click{frame, 1, down, true, false, p.x, p.y, yv});
        }
        m.gameDown = down;
        GJBaseGameLayer::handleButton(down, 1, true);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto& m = MacroManager::get();
        bool inLevel = PlayLayer::get() != nullptr;
        bool isMain = inLevel && button == 1 && isPlayer1;

        // Selalu ingat posisi tombol layar yang sebenarnya (untuk sinkron saat bot berhenti)
        if (isMain) m.realDown = down;

        // Saat playback, sentuhan pemain diabaikan (hanya klik dari macro)
        if (m.state == MacroManager::State::Playing && !m.injecting) return;

        // Auto Clicker / Auto Slope: bot yang menentukan tombol (lihat kbDrive)
        if (isMain && m.drivesInput() && !m.injecting) return;

        // Saat rekam, simpan setiap klik beserta nomor step & posisi player
        if (m.state == MacroManager::State::Recording && !m.injecting && inLevel) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);
            auto* pl = isPlayer1 ? m_player1 : m_player2;
            CCPoint p = pl ? pl->getPosition() : CCPoint{0.f, 0.f};
            double yv = pl ? pl->m_yVelocity : 0.0;
            m.clicks.push_back(Click{frame, button, down, isPlayer1, false, p.x, p.y, yv});
        }

        if (isMain) m.gameDown = down;
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    // ----------------------------------------------------------
    // Auto Slope Wave: mengembalikan tombol yang diinginkan,
    // atau nullopt kalau tidak ada slope relevan (kontrol balik ke pemain)
    // ----------------------------------------------------------
    std::optional<bool> kbSlope(int frame) {
        auto& m = MacroManager::get();
        auto* p = m_player1;
        if (!p || !p->m_isDart) {          // hanya aktif di mode wave
            m.lastAction = -1;
            return std::nullopt;
        }

        // Cache semua objek slope di level
        if (m.slopeOwner != this || !m_objects || m.slopeObjCount != m_objects->count()) {
            m.slopes.clear();
            m.slopeOwner = this;
            m.slopeObjCount = m_objects ? m_objects->count() : 0;
            if (m_objects) {
                for (auto* obj : CCArrayExt<GameObject*>(m_objects)) {
                    if (obj && obj->m_objectType == GameObjectType::Slope) m.slopes.push_back(obj);
                }
            }
        }
        if (m.slopes.empty()) {
            m.lastAction = -1;
            return std::nullopt;
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
        if (m.lastAction >= 0 && frame != m.lastFrame) {
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
        m.lastFrame = frame;

        // ---- Cari slope terdekat di bawah & di atas wave ----
        const double L = static_cast<double>(MacroManager::slopeLook());
        const double ahead = L * 0.35;      // titik sampling di depan wave
        const double window = 70.0;         // jarak vertikal maksimum slope yang dianggap relevan
        const double margin = mini ? 5.0 : 8.0;

        bool haveBelow = false, haveAbove = false;
        double belowY = 0, belowTan = 0, aboveY = 0, aboveTan = 0;   // garis slope yang diproyeksikan ke x pemain

        for (auto* s : m.slopes) {
            if (!s || s->m_isNoTouch || s->m_isPassable) continue;
            auto const& r = s->getObjectRect();
            double minX = r.getMinX(), maxX = r.getMaxX();
            if (maxX < pos.x - 2.0 || minX > pos.x + L) continue;

            double xe = std::clamp(pos.x + ahead, minX, maxX);
            double ys = s->slopeYPos(static_cast<float>(xe));
            if (!std::isfinite(ys)) continue;
            ys = std::clamp(ys, static_cast<double>(r.getMinY()) - 2.0, static_cast<double>(r.getMaxY()) + 2.0);

            double xa = std::clamp(xe - 4.0, minX, maxX);
            double xb = std::clamp(xe + 4.0, minX, maxX);
            double tn = (xb - xa) > 0.5
                ? (s->slopeYPos(static_cast<float>(xb)) - s->slopeYPos(static_cast<float>(xa))) / (xb - xa)
                : 0.0;
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
            return std::nullopt;
        }

        // ---- Hitung rasio tahan/lepas (PWM) supaya rata-rata mengikuti slope ----
        double dHold = (tanT - m.kRel) / (m.kHold - m.kRel);          // feed-forward dari kemiringan
        double sgn = (m.kHold > m.kRel) ? 1.0 : -1.0;
        dHold += sgn * (target - pos.y) / 12.0;                         // koreksi posisi
        dHold = std::clamp(dHold, 0.0, 1.0);

        int T = std::max(2, static_cast<int>(std::lround(MacroManager::TPS / MacroManager::slopeCps())));
        int holdSteps = static_cast<int>(std::lround(dHold * T));
        bool desired = (frame % T) < holdSteps;

        m.lastAction = desired ? 1 : 0;
        return desired;
    }

    // ----------------------------------------------------------
    // Tentukan tombol yang seharusnya ditekan tiap step
    // (Auto Slope > Auto Clicker > sentuhan pemain)
    // Juga menyinkronkan tombol saat bot berhenti, supaya tidak ada tombol "nyangkut".
    // ----------------------------------------------------------
    void kbDrive(int frame) {
        auto& m = MacroManager::get();
        bool desired = m.realDown;

        bool controlled = false;
        if (m.slopeOn) {
            if (auto d = this->kbSlope(frame)) {
                desired = *d;
                controlled = true;
                m.tainted = true;
            }
        }
        if (!controlled && m.acOn && m.realDown) {
            int P = std::max(2, static_cast<int>(std::lround(MacroManager::TPS / MacroManager::acCps())));
            int holdSteps = (P + 1) / 2;
            desired = (frame % P) < holdSteps;
            m.tainted = true;
        }

        if (desired != m.gameDown) this->kbSend(desired);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& m = MacroManager::get();

        if (PlayLayer::get() && m_player1) {
            int frame = static_cast<int>(m_gameState.m_currentProgress);

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
                    auto* pl = c.p1 ? m_player1 : m_player2;

                    // Koreksi posisi hanya kalau step-nya persis sama dengan saat rekam
                    if (pl && c.frame == frame && (c.px != 0.f || c.py != 0.f)) {
                        pl->setPosition(CCPoint{c.px, c.py});
                        pl->m_yVelocity = c.yv;
                    }

                    if (!c.snap) {
                        if (c.button == 1 && c.p1) m.gameDown = c.down;
                        GJBaseGameLayer::handleButton(c.down, c.button, c.p1);
                    }
                }
                m.injecting = false;

                // Macro habis -> bot berhenti, kontrol kembali ke pemain
                if (m.index >= m.clicks.size()) {
                    m.stop();
                    Notification::create("Macro selesai, kontrol kembali ke kamu", NotificationIcon::Info)->show();
                }
            } else {
                // ---- Auto Clicker / Auto Slope / sinkron tombol ----
                this->kbDrive(frame);
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
