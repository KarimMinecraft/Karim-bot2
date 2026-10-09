#pragma once
#include "MacroManager.hpp"
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/NineSlice.hpp>

// ============================================================
// Popup daftar macro tersimpan: Load & Hapus
// Nama macro = nama level (hasil autosave)
// ============================================================
class MacroListPopup : public geode::Popup {
protected:
    ScrollLayer* m_scroll = nullptr;

    bool init() {
        if (!Popup::init(340.f, 230.f)) return false;

        this->setTitle("Karim Bot - Macro Tersimpan");
        this->refresh();
        return true;
    }

    void refresh() {
        if (m_scroll) {
            m_scroll->removeFromParent();
            m_scroll = nullptr;
        }
        if (auto old = m_mainLayer->getChildByTag(9001)) old->removeFromParent();

        auto names = MacroManager::get().list();
        const float w = 300.f, h = 160.f, rowH = 38.f;

        if (names.empty()) {
            auto empty = CCLabelBMFont::create("Belum ada macro.\nTekan Record lalu main sampai selesai,\nmacro otomatis tersimpan di sini.", "bigFont.fnt");
            empty->setScale(0.34f);
            empty->setPosition({m_size.width / 2, m_size.height / 2 - 5.f});
            empty->setTag(9001);
            m_mainLayer->addChild(empty);
            return;
        }

        m_scroll = ScrollLayer::create(CCSize{w, h});
        m_scroll->setPosition({(m_size.width - w) / 2, 22.f});

        float total = std::max(h, rowH * static_cast<float>(names.size()));
        m_scroll->m_contentLayer->setContentSize({w, total});

        for (size_t i = 0; i < names.size(); i++) {
            std::string name = names[i];
            int clicks = MacroManager::get().clickCount(name);
            float y = total - rowH * static_cast<float>(i + 1);

            auto row = CCNode::create();
            row->setPosition({0, y});
            row->setContentSize({w, rowH});

            // latar baris (gelap, agak transparan)
            auto bg = NineSlice::create("square02_001.png");
            bg->setContentSize({w - 6.f, rowH - 5.f});
            bg->setColor({0, 0, 0});
            bg->setOpacity(120);
            bg->setPosition({w / 2, rowH / 2});
            row->addChild(bg);

            auto label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            label->limitLabelWidth(150.f, 0.5f, 0.2f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({14.f, rowH / 2 + 6.f});
            row->addChild(label);

            auto info = CCLabelBMFont::create((std::to_string(clicks) + " klik").c_str(), "chatFont.fnt");
            info->setScale(0.6f);
            info->setColor({150, 200, 255});
            info->setAnchorPoint({0.f, 0.5f});
            info->setPosition({14.f, rowH / 2 - 9.f});
            row->addChild(info);

            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            menu->setContentSize({w, rowH});

            auto loadBtn = CCMenuItemExt::createSpriteExtra(
                ButtonSprite::create("Load", 60, true, "goldFont.fnt", "GJ_button_01.png", 24.f, 0.6f),
                [this, name](CCObject*) {
                    if (MacroManager::get().load(name)) {
                        Notification::create("Macro dimuat: " + name, NotificationIcon::Success)->show();
                        this->onClose(nullptr);
                    } else {
                        Notification::create("Gagal memuat macro", NotificationIcon::Error)->show();
                    }
                }
            );
            loadBtn->setPosition({w - 78.f, rowH / 2});
            menu->addChild(loadBtn);

            auto delBtn = CCMenuItemExt::createSpriteExtra(
                ButtonSprite::create("Hapus", 52, true, "goldFont.fnt", "GJ_button_06.png", 24.f, 0.6f),
                [this, name](CCObject*) {
                    MacroManager::get().remove(name);
                    this->refresh();
                }
            );
            delBtn->setPosition({w - 28.f, rowH / 2});
            menu->addChild(delBtn);

            row->addChild(menu);
            m_scroll->m_contentLayer->addChild(row);
        }

        m_scroll->moveToTop();
        m_mainLayer->addChild(m_scroll);
    }

public:
    static MacroListPopup* create() {
        auto ret = new MacroListPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
