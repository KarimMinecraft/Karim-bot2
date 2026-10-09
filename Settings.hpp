#pragma once
#include "MacroManager.hpp"
#include "Popups.hpp"
#include <functional>

// ============================================================
// Popup "Opsi": Auto Clicker, Auto Slope Wave, Akurasi macro
// ============================================================
class KBSettingsPopup : public geode::Popup {
protected:
    CCMenu* m_menu = nullptr;

    CCLabelBMFont* addText(char const* text, float x, float y, float maxW, float scale, ccColor3B color = {255, 255, 255}) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->limitLabelWidth(maxW, scale, 0.15f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({x, y});
        label->setColor(color);
        m_mainLayer->addChild(label, 3);
        return label;
    }

    void addDivider(float y) {
        auto line = CCLayerColor::create({255, 255, 255, 40}, m_size.width - 36.f, 1.f);
        line->setPosition({18.f, y});
        m_mainLayer->addChild(line, 2);
    }

    // Checkbox + judul
    void addToggle(char const* title, float y, bool value, std::function<void(bool)> onChange) {
        auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [onChange](CCMenuItemToggler* t) {
            onChange(!t->isToggled());   // callback jalan sebelum state berubah
        });
        toggle->toggle(value);
        toggle->setPosition({32.f, y});
        m_menu->addChild(toggle);

        addText(title, 54.f, y, 250.f, 0.45f, {255, 215, 120});
    }

    // Baris pengaturan angka: [-10] [-1] nilai [+1] [+10] ([MAX])
    void addStepper(char const* title, float y, std::function<int()> get, std::function<void(int)> set,
                    int minV, int maxV, int small, int big, bool maxButton) {
        addText(title, 18.f, y, 150.f, 0.36f);

        auto value = CCLabelBMFont::create("", "goldFont.fnt");
        value->setPosition({268.f, y});
        m_mainLayer->addChild(value, 3);

        auto refresh = [value, get, maxV, maxButton]() {
            int v = get();
            std::string text = (maxButton && v >= maxV) ? std::string("MAX") : std::to_string(v);
            value->setString(text.c_str());
            value->limitLabelWidth(40.f, 0.5f, 0.2f);
        };
        refresh();

        auto addBtn = [&](char const* text, float x, int delta) {
            auto btn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode(text, 30.f, 22.f, "GJ_button_02.png", 0.45f),
                [get, set, delta, refresh](CCObject*) {
                    set(get() + delta);
                    refresh();
                }
            );
            btn->setPosition({x, y});
            m_menu->addChild(btn);
        };
        addBtn(("-" + std::to_string(big)).c_str(), 190.f, -big);
        addBtn(("-" + std::to_string(small)).c_str(), 224.f, -small);
        addBtn(("+" + std::to_string(small)).c_str(), 312.f, small);
        addBtn(("+" + std::to_string(big)).c_str(), 346.f, big);

        if (maxButton) {
            auto maxBtn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode("MAX", 36.f, 22.f, "GJ_button_06.png", 0.45f),
                [set, maxV, refresh](CCObject*) {
                    set(maxV);
                    refresh();
                }
            );
            maxBtn->setPosition({379.f, y});
            m_menu->addChild(maxBtn);
        }
        (void)minV;
    }

    bool init() {
        if (!Popup::init(404.f, 268.f)) return false;
        this->setTitle("Karot Utils - Opsi");

        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_menu->setContentSize(m_size);
        m_mainLayer->addChild(m_menu, 5);

        // ---------- Auto Clicker ----------
        addToggle("Auto Clicker", 214.f, MacroManager::get().acOn, [](bool on) {
            MacroManager::get().acOn = on;
            Notification::create(on ? "Auto Clicker ON" : "Auto Clicker OFF",
                                 on ? NotificationIcon::Success : NotificationIcon::Info)->show();
        });
        addStepper("Klik per detik", 186.f,
                   [] { return MacroManager::acCps(); },
                   [](int v) { MacroManager::setAcCps(v); },
                   1, 120, 1, 10, true);

        addDivider(166.f);

        // ---------- Auto Slope Wave ----------
        addToggle("Auto Slope Wave (Beta)", 148.f, MacroManager::get().slopeOn, [](bool on) {
            auto& m = MacroManager::get();
            m.slopeOn = on;
            m.resetSlopeCalibration();
            Notification::create(on ? "Auto Slope Wave ON (khusus mode wave)" : "Auto Slope Wave OFF",
                                 on ? NotificationIcon::Success : NotificationIcon::Info)->show();
        });
        addStepper("Kecepatan klik", 120.f,
                   [] { return MacroManager::slopeCps(); },
                   [](int v) { MacroManager::setSlopeCps(v); },
                   10, 120, 1, 10, true);
        addStepper("Jarak pandang", 92.f,
                   [] { return MacroManager::slopeLook(); },
                   [](int v) { MacroManager::setSlopeLook(v); },
                   30, 300, 10, 30, false);

        addDivider(72.f);

        // ---------- Akurasi macro ----------
        addStepper("Koreksi tiap N step", 52.f,
                   [] { return MacroManager::snapInterval(); },
                   [](int v) { MacroManager::setSnapInterval(v); },
                   1, 20, 1, 5, false);

        addText("Auto Clicker: tahan layar = spam klik. Slope Wave: khusus mode wave.",
                18.f, 32.f, m_size.width - 36.f, 0.27f, {150, 165, 205});
        addText("Angka koreksi kecil = macro lebih akurat (rekaman berikutnya).",
                18.f, 21.f, m_size.width - 36.f, 0.27f, {150, 165, 205});
        addText("Bot aktif -> Safe Mode tetap menahan progress.",
                18.f, 10.f, m_size.width - 36.f, 0.27f, {150, 165, 205});
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
