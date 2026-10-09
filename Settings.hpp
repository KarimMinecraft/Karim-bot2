#pragma once
#include "MacroManager.hpp"
#include "Popups.hpp"
#include <Geode/ui/ScrollLayer.hpp>
#include <functional>

// ============================================================
// Popup "Opsi" (bisa di-scroll):
//   Auto Clicker, Auto Slope Wave, Tampilan (hitbox), Speed Hack, Macro
// ============================================================
class KBSettingsPopup : public geode::Popup {
protected:
    CCNode* m_content = nullptr;   // isi yang discroll (y negatif ke bawah, digeser saat selesai)
    CCMenu* m_menu = nullptr;      // semua tombol
    float m_cursor = 0.f;          // posisi y saat ini (turun ke bawah)

    static constexpr float CW = 372.f;     // lebar isi
    static constexpr float ROW = 30.f;     // tinggi satu baris

    CCLabelBMFont* addText(char const* text, float x, float y, float maxW, float scale, ccColor3B color = {255, 255, 255}) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->limitLabelWidth(maxW, scale, 0.15f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({x, y});
        label->setColor(color);
        m_content->addChild(label, 3);
        return label;
    }

    // Judul bagian + garis aksen emas
    void addSection(char const* title) {
        m_cursor -= 10.f;
        auto label = CCLabelBMFont::create(title, "goldFont.fnt");
        label->limitLabelWidth(CW - 20.f, 0.5f, 0.25f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({10.f, m_cursor - 8.f});
        m_content->addChild(label, 3);

        auto line = CCLayerColor::create({255, 205, 90, 120}, CW - 20.f, 1.f);
        line->setPosition({10.f, m_cursor - 19.f});
        m_content->addChild(line, 2);
        m_cursor -= 26.f;
    }

    void addHint(char const* text) {
        auto label = CCLabelBMFont::create(text, "chatFont.fnt");
        label->limitLabelWidth(CW - 24.f, 0.55f, 0.3f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setColor({150, 165, 205});
        label->setPosition({12.f, m_cursor - 8.f});
        m_content->addChild(label, 3);
        m_cursor -= 16.f;
    }

    // Baris latar gelap supaya tiap baris rapi
    void addRowBg(float y) {
        auto bg = kbui::pill(CW - 8.f, ROW - 4.f, {0, 0, 0}, 70);
        bg->setPosition({CW / 2, y});
        m_content->addChild(bg, 1);
    }

    // Checkbox + judul
    void addToggle(char const* title, bool value, std::function<void(bool)> onChange) {
        float y = m_cursor - ROW / 2;
        addRowBg(y);

        auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [onChange](CCMenuItemToggler* t) {
            onChange(!t->isToggled());   // callback jalan sebelum state berubah
        });
        toggle->toggle(value);
        toggle->setPosition({26.f, y});
        m_menu->addChild(toggle);

        addText(title, 46.f, y, CW - 60.f, 0.42f, {255, 225, 140});
        m_cursor -= ROW;
    }

    // Baris angka: judul, [-besar] [-kecil] nilai [+kecil] [+besar] ([MAX])
    void addStepper(char const* title, std::function<int()> get, std::function<void(int)> set,
                    int smallStep, int bigStep, bool maxButton, int maxV, char const* suffix) {
        float y = m_cursor - ROW / 2;
        addRowBg(y);
        addText(title, 12.f, y, 112.f, 0.36f);

        auto value = CCLabelBMFont::create("", "goldFont.fnt");
        value->setPosition({224.f, y});
        m_content->addChild(value, 3);

        std::string suf = suffix ? suffix : "";
        auto refresh = [value, get, maxV, maxButton, suf]() {
            int v = get();
            std::string text = (maxButton && v >= maxV) ? std::string("MAX") : std::to_string(v) + suf;
            value->setString(text.c_str());
            value->limitLabelWidth(46.f, 0.5f, 0.2f);
        };
        refresh();

        auto addBtn = [this, y, get, set, refresh](std::string const& text, float x, int delta) {
            auto btn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode(text.c_str(), 30.f, 22.f, "GJ_button_02.png", 0.45f),
                [get, set, delta, refresh](CCObject*) {
                    set(get() + delta);
                    refresh();
                }
            );
            btn->setPosition({x, y});
            m_menu->addChild(btn);
        };
        addBtn("-" + std::to_string(bigStep), 148.f, -bigStep);
        addBtn("-" + std::to_string(smallStep), 180.f, -smallStep);
        addBtn("+" + std::to_string(smallStep), 268.f, smallStep);
        addBtn("+" + std::to_string(bigStep), 300.f, bigStep);

        if (maxButton) {
            auto maxBtn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode("MAX", 38.f, 22.f, "GJ_button_06.png", 0.45f),
                [set, maxV, refresh](CCObject*) {
                    set(maxV);
                    refresh();
                }
            );
            maxBtn->setPosition({342.f, y});
            m_menu->addChild(maxBtn);
        }
        m_cursor -= ROW;
    }

    bool init() {
        if (!Popup::init(404.f, 268.f)) return false;
        this->setTitle("Karot Utils - Opsi");

        const float viewW = 380.f, viewH = 214.f;

        m_content = CCNode::create();
        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_content->addChild(m_menu, 5);

        // ---------- Auto Clicker ----------
        addSection("Auto Clicker");
        addToggle("Auto Clicker (langsung spam)", MacroManager::get().acOn, [](bool on) {
            MacroManager::get().acOn = on;
            Notification::create(on ? "Auto Clicker ON" : "Auto Clicker OFF",
                                 on ? NotificationIcon::Success : NotificationIcon::Info)->show();
        });
        addStepper("Klik per detik",
                   [] { return MacroManager::acCps(); },
                   [](int v) { MacroManager::setAcCps(v); },
                   1, 10, true, 120, "");
        addToggle("Hanya saat layar ditahan", MacroManager::get().acHoldOnly, [](bool on) {
            MacroManager::get().acHoldOnly = on;
        });

        // ---------- Auto Slope Wave ----------
        addSection("Auto Slope Wave");
        addToggle("Auto Slope Wave (khusus wave)", MacroManager::get().slopeOn, [](bool on) {
            auto& m = MacroManager::get();
            m.slopeOn = on;
            m.resetSlopeCalibration();
            Notification::create(on ? "Auto Slope Wave ON" : "Auto Slope Wave OFF",
                                 on ? NotificationIcon::Success : NotificationIcon::Info)->show();
        });
        addStepper("Kecepatan klik",
                   [] { return MacroManager::slopeCps(); },
                   [](int v) { MacroManager::setSlopeCps(v); },
                   1, 10, true, 120, "");
        addStepper("Jarak pandang",
                   [] { return MacroManager::slopeLook(); },
                   [](int v) { MacroManager::setSlopeLook(v); },
                   10, 30, false, 300, "");
        addHint("Mini/besar, slope lonjong, dan 45 derajat dihitung otomatis.");

        // ---------- Tampilan ----------
        addSection("Tampilan");
        addToggle("Show Hitboxes", MacroManager::hitboxes(), [](bool on) {
            MacroManager::setHitboxes(on);
            auto* gm = GameManager::get();
            if (gm) gm->setGameVariable("0166", on);
            if (auto* pl = PlayLayer::get()) {
                pl->updateDebugDrawSettings();
                pl->m_isDebugDrawEnabled = on;
                if (pl->m_debugDrawNode) {
                    if (!on) pl->m_debugDrawNode->clear();
                    pl->m_debugDrawNode->setVisible(on);
                }
            }
        });
        addToggle("Info bot di layar (HUD kecil)", MacroManager::hud(), [](bool on) {
            MacroManager::setHud(on);
        });

        // ---------- Speed Hack ----------
        addSection("Speed Hack");
        addToggle("Speed Hack", MacroManager::get().speedOn, [](bool on) {
            MacroManager::get().speedOn = on;
            Notification::create(on ? "Speed Hack ON (Safe Mode menahan progress)" : "Speed Hack OFF",
                                 on ? NotificationIcon::Warning : NotificationIcon::Info)->show();
        });
        addStepper("Kecepatan (%)",
                   [] { return MacroManager::speedPercent(); },
                   [](int v) { MacroManager::setSpeedPercent(v); },
                   5, 25, false, 300, "%");
        addStepper("Kehalusan",
                   [] { return MacroManager::speedSmooth(); },
                   [](int v) { MacroManager::setSpeedSmooth(v); },
                   1, 3, false, 10, "");
        addHint("Berubah pelan-pelan (mulus). Kehalusan besar = lebih halus.");

        // ---------- Macro ----------
        addSection("Akurasi Macro");
        addStepper("Koreksi tiap N step",
                   [] { return MacroManager::snapInterval(); },
                   [](int v) { MacroManager::setSnapInterval(v); },
                   1, 5, false, 20, "");
        addHint("Angka kecil = macro lebih akurat (berlaku untuk rekaman berikutnya).");
        addHint("Bot aktif atau speed diubah -> Safe Mode menahan progress.");

        // ---------- pasang ke ScrollLayer ----------
        float used = -m_cursor + 10.f;
        float total = std::max(viewH, used);
        m_content->setPositionY(total);   // y negatif -> koordinat di dalam layer scroll

        auto scroll = ScrollLayer::create(CCSize{viewW, viewH});
        scroll->setPosition({(m_size.width - viewW) / 2, 14.f});
        scroll->m_contentLayer->setContentSize({viewW, total});
        m_content->setPositionX((viewW - CW) / 2);
        scroll->m_contentLayer->addChild(m_content);
        scroll->moveToTop();
        m_mainLayer->addChild(scroll, 4);
        return true;
    }

public:
    static KBSettingsPopup* create() {
        auto ret = new KBSettingsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
