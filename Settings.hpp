#pragma once
#include "MacroManager.hpp"
#include "Popups.hpp"
#include <functional>
#include <vector>

// ============================================================
// Popup "Opsi" dengan 5 TAB (tanpa scroll):
//   Clicker | Slope | Tampilan | Speed | Macro
// ============================================================
class KBSettingsPopup : public geode::Popup {
protected:
    static constexpr float CW = 372.f;      // lebar isi
    static constexpr float ROWH = 34.f;     // tinggi satu baris

    CCNode* m_page = nullptr;               // halaman yang sedang dibangun
    CCMenu* m_menu = nullptr;               // menu tombol halaman itu
    float m_cursor = 0.f;                   // posisi y baris berikutnya (turun ke bawah)
    std::vector<CCNode*> m_pages;
    std::vector<CCMenu*> m_menus;
    CCMenu* m_tabMenu = nullptr;
    int m_tab = 0;

    // Mulai halaman baru: node kosong + menu sendiri
    void beginPage() {
        m_page = CCNode::create();
        m_page->setPosition({(m_size.width - CW) / 2, 0.f});
        m_page->setVisible(false);
        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_page->addChild(m_menu, 5);
        m_mainLayer->addChild(m_page, 4);
        m_pages.push_back(m_page);
        m_menus.push_back(m_menu);
        m_cursor = 204.f;
    }

    CCLabelBMFont* addText(char const* text, float x, float y, float maxW, float scale, ccColor3B color = {255, 255, 255}) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->limitLabelWidth(maxW, scale, 0.15f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({x, y});
        label->setColor(color);
        m_page->addChild(label, 3);
        return label;
    }

    void addHint(char const* text) {
        auto label = CCLabelBMFont::create(text, "chatFont.fnt");
        label->limitLabelWidth(CW - 24.f, 0.55f, 0.3f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setColor({150, 165, 205});
        label->setPosition({12.f, m_cursor - 8.f});
        m_page->addChild(label, 3);
        m_cursor -= 18.f;
    }

    // Latar gelap tiap baris supaya rapi
    void addRowBg(float y) {
        auto bg = kbui::pill(CW - 8.f, ROWH - 4.f, {0, 0, 0}, 70);
        bg->setPosition({CW / 2, y});
        m_page->addChild(bg, 1);
    }

    // Checkbox + judul
    void addToggle(char const* title, bool value, std::function<void(bool)> onChange) {
        float y = m_cursor - ROWH / 2;
        addRowBg(y);

        auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [onChange](CCMenuItemToggler* t) {
            onChange(!t->isToggled());   // callback jalan sebelum state berubah
        });
        toggle->toggle(value);
        toggle->setPosition({26.f, y});
        m_menu->addChild(toggle);

        addText(title, 46.f, y, CW - 60.f, 0.42f, {255, 225, 140});
        m_cursor -= ROWH;
    }

    // Baris angka: judul, [-besar] [-kecil] nilai [+kecil] [+besar] ([MAX])
    void addStepper(char const* title, std::function<int()> get, std::function<void(int)> set,
                    int smallStep, int bigStep, bool maxButton, int maxV, char const* suffix) {
        float y = m_cursor - ROWH / 2;
        addRowBg(y);
        addText(title, 12.f, y, 112.f, 0.36f);

        auto value = CCLabelBMFont::create("", "goldFont.fnt");
        value->setPosition({224.f, y});
        m_page->addChild(value, 3);

        std::string suf = suffix ? suffix : "";
        auto refresh = [value, get, maxV, maxButton, suf]() {
            int v = get();
            std::string text = (maxButton && v >= maxV) ? std::string("MAX") : (std::to_string(v) + suf);
            value->setString(text.c_str());
            value->limitLabelWidth(46.f, 0.5f, 0.2f);
        };
        refresh();

        CCMenu* menu = m_menu;
        auto addBtn = [menu, y, get, set, refresh](std::string const& text, float x, int delta) {
            auto btn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode(text.c_str(), 30.f, 22.f, "GJ_button_02.png", 0.45f),
                [get, set, delta, refresh](CCObject*) {
                    set(get() + delta);
                    refresh();
                }
            );
            btn->setPosition({x, y});
            menu->addChild(btn);
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
        m_cursor -= ROWH;
    }

    void showTab(int idx) {
        m_tab = idx;
        for (size_t i = 0; i < m_pages.size(); i++) {
            bool on = static_cast<int>(i) == idx;
            m_pages[i]->setVisible(on);
            m_menus[i]->setEnabled(on);
        }
        this->buildTabs();
    }

    void buildTabs() {
        if (m_tabMenu) m_tabMenu->removeFromParent();
        m_tabMenu = CCMenu::create();
        m_tabMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_tabMenu, 10);

        char const* names[5] = {"Clicker", "Slope", "Tampilan", "Speed", "Macro"};
        const float tw = 70.f, th = 24.f, gap = 4.f;
        float total = 5.f * tw + 4.f * gap;
        float x0 = (m_size.width - total) / 2 + tw / 2;
        for (int i = 0; i < 5; i++) {
            bool sel = (i == m_tab);
            auto btn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode(names[i], tw, th, sel ? "GJ_button_01.png" : "GJ_button_04.png", 0.42f),
                [this, i](CCObject*) { this->showTab(i); }
            );
            btn->setPosition({x0 + static_cast<float>(i) * (tw + gap), m_size.height - 52.f});
            m_tabMenu->addChild(btn);
        }
    }

    bool init() {
        if (!Popup::init(404.f, 268.f)) return false;
        this->setTitle("Karot Utils - Opsi");

        // ---------- Tab 1: Auto Clicker ----------
        beginPage();
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
        addHint("Nyalakan, tutup menu, lanjut main: bot langsung spam.");

        // ---------- Tab 2: Auto Slope Wave ----------
        beginPage();
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

        // ---------- Tab 3: Tampilan ----------
        beginPage();
        addToggle("Show Hitboxes", MacroManager::hitboxes(), [](bool on) {
            MacroManager::setHitboxes(on);
        });
        addToggle("Info bot di layar (HUD kecil)", MacroManager::hud(), [](bool on) {
            MacroManager::setHud(on);
        });

        // ---------- Tab 4: Speed Hack ----------
        beginPage();
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

        // ---------- Tab 5: Macro ----------
        beginPage();
        addStepper("Koreksi tiap N step",
                   [] { return MacroManager::snapInterval(); },
                   [](int v) { MacroManager::setSnapInterval(v); },
                   1, 5, false, 20, "");
        addHint("Angka kecil = macro lebih akurat (untuk rekaman berikutnya).");
        addHint("Bot aktif atau speed diubah -> Safe Mode menahan progress.");

        showTab(0);
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
