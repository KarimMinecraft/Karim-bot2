#pragma once
#include "MacroManager.hpp"
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Notification.hpp>

// ============================================================
// Popup untuk memberi nama & menyimpan macro setelah rekaman
// ============================================================
class SaveMacroPopup : public geode::Popup<> {
protected:
    TextInput* m_input = nullptr;

    bool setup() override {
        this->setTitle("Simpan Macro");

        m_input = TextInput::create(220.f, "Nama macro", "bigFont.fnt");
        m_input->setString("macro1");
        m_input->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ");
        m_input->setMaxCharCount(24);
        m_input->setPosition({m_size.width / 2, m_size.height / 2 + 5.f});
        m_mainLayer->addChild(m_input);

        auto btn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Simpan"),
            [this](CCObject*) {
                auto name = m_input->getString();
                if (name.empty()) name = "macro";
                if (MacroManager::get().save(name)) {
                    Notification::create("Macro disimpan: " + MacroManager::get().loadedName,
                                         NotificationIcon::Success)->show();
                } else {
                    Notification::create("Gagal menyimpan macro", NotificationIcon::Error)->show();
                }
                this->onClose(nullptr);
            }
        );
        btn->setPosition({m_size.width / 2, 35.f});
        m_buttonMenu->addChild(btn);
        return true;
    }

public:
    static SaveMacroPopup* create() {
        auto ret = new SaveMacroPopup();
        if (ret && ret->initAnd(300.f, 150.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// ============================================================
// Popup daftar macro tersimpan: Load & Hapus
// ============================================================
class MacroListPopup : public geode::Popup<> {
protected:
    ScrollLayer* m_scroll = nullptr;

    bool setup() override {
        this->setTitle("Macro Tersimpan");
        this->refresh();
        return true;
    }

    void refresh() {
        if (m_scroll) {
            m_scroll->removeFromParent();
            m_scroll = nullptr;
        }

        auto names = MacroManager::get().list();
        const float w = 260.f, h = 150.f, rowH = 34.f;

        if (names.empty()) {
            auto empty = CCLabelBMFont::create("Belum ada macro.\nRekam dulu di practice mode!", "bigFont.fnt");
            empty->setScale(0.4f);
            empty->setPosition({m_size.width / 2, m_size.height / 2});
            empty->setTag(9001);
            if (auto old = m_mainLayer->getChildByTag(9001)) old->removeFromParent();
            m_mainLayer->addChild(empty);
            return;
        }
        if (auto old = m_mainLayer->getChildByTag(9001)) old->removeFromParent();

        m_scroll = ScrollLayer::create({w, h});
        m_scroll->setPosition({(m_size.width - w) / 2, 25.f});

        float total = std::max(h, rowH * names.size());
        m_scroll->m_contentLayer->setContentSize({w, total});

        for (size_t i = 0; i < names.size(); i++) {
            auto name = names[i];
            float y = total - rowH * (i + 1);

            auto row = CCNode::create();
            row->setPosition({0, y});
            row->setContentSize({w, rowH});

            auto label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            label->limitLabelWidth(130.f, 0.5f, 0.2f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({10.f, rowH / 2});
            row->addChild(label);

            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            menu->setContentSize({w, rowH});

            auto loadBtn = CCMenuItemExt::createSpriteExtra(
                ButtonSprite::create("Load", "goldFont.fnt", "GJ_button_01.png", 0.6f),
                [this, name](CCObject*) {
                    if (MacroManager::get().load(name)) {
                        Notification::create("Macro dimuat: " + name, NotificationIcon::Success)->show();
                        this->onClose(nullptr);
                    } else {
                        Notification::create("Gagal memuat macro", NotificationIcon::Error)->show();
                    }
                }
            );
            loadBtn->setPosition({w - 80.f, rowH / 2});
            menu->addChild(loadBtn);

            auto delBtn = CCMenuItemExt::createSpriteExtra(
                ButtonSprite::create("Hapus", "goldFont.fnt", "GJ_button_06.png", 0.6f),
                [this, name](CCObject*) {
                    MacroManager::get().remove(name);
                    this->refresh();
                }
            );
            delBtn->setPosition({w - 30.f, rowH / 2});
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
        if (ret && ret->initAnd(300.f, 210.f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
