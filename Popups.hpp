#pragma once
#include "MacroManager.hpp"
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/NineSlice.hpp>

namespace kbui {

    // Tombol buatan sendiri: ukuran & teks selalu seimbang (teks otomatis diperkecil kalau kepanjangan).
    // bg: "GJ_button_01.png" (hijau), "GJ_button_02.png" (biru), "GJ_button_06.png" (merah), dst.
    inline CCNode* buttonNode(char const* text, float w, float h, char const* bg, float textScale = 0.55f) {
        auto node = CCNode::create();
        node->setContentSize({w, h});

        auto spr = NineSlice::create(bg, CCRect{0.f, 0.f, 40.f, 40.f}, NineSlice::Insets{12.f, 12.f, 12.f, 12.f});
        if (spr) {
            spr->setContentSize({w, h});
            spr->setPosition({w / 2, h / 2});
            node->addChild(spr, 0);
        }

        auto label = CCLabelBMFont::create(text, "goldFont.fnt");
        label->limitLabelWidth(w - 16.f, textScale, 0.2f);
        label->setPosition({w / 2, h / 2 + 1.f});
        node->addChild(label, 1);
        return node;
    }

    // Pil/kapsul gelap untuk label status
    inline NineSlice* pill(float w, float h, ccColor3B color, GLubyte opacity) {
        auto bg = NineSlice::create("square02_001.png");
        bg->setContentSize({w, h});
        bg->setColor(color);
        bg->setOpacity(opacity);
        return bg;
    }
}

// ============================================================
// Popup daftar macro tersimpan: Load & Hapus
// Nama macro = nama level (hasil autosave)
// ============================================================
class MacroListPopup : public geode::Popup {
protected:
    ScrollLayer* m_scroll = nullptr;

    bool init() {
        if (!Popup::init(350.f, 232.f)) return false;

        this->setTitle("Macro Tersimpan");
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
        const float w = 310.f, h = 164.f, rowH = 44.f;

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

            // latar baris
            auto bg = kbui::pill(w - 6.f, rowH - 6.f, {0, 0, 0}, 120);
            bg->setPosition({w / 2, rowH / 2});
            row->addChild(bg);

            // nama macro + jumlah klik (rata kiri, dua baris)
            auto label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            label->limitLabelWidth(150.f, 0.5f, 0.2f);
            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({16.f, rowH / 2 + 7.f});
            row->addChild(label);

            auto info = CCLabelBMFont::create((std::to_string(clicks) + " klik").c_str(), "chatFont.fnt");
            info->setScale(0.62f);
            info->setColor({150, 200, 255});
            info->setAnchorPoint({0.f, 0.5f});
            info->setPosition({16.f, rowH / 2 - 9.f});
            row->addChild(info);

            // dua tombol: ukuran SAMA, tinggi SAMA, sejajar di tengah baris
            const float bw = 62.f, bh = 28.f;
            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            menu->setContentSize({w, rowH});

            auto loadBtn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode("Load", bw, bh, "GJ_button_01.png", 0.55f),
                [this, name](CCObject*) {
                    if (MacroManager::get().load(name)) {
                        Notification::create("Macro dimuat: " + name, NotificationIcon::Success)->show();
                        this->onClose(nullptr);
                    } else {
                        Notification::create("Gagal memuat macro", NotificationIcon::Error)->show();
                    }
                }
            );
            loadBtn->setPosition({w - 16.f - bw * 1.5f - 6.f, rowH / 2});
            menu->addChild(loadBtn);

            auto delBtn = CCMenuItemExt::createSpriteExtra(
                kbui::buttonNode("Hapus", bw, bh, "GJ_button_06.png", 0.55f),
                [this, name](CCObject*) {
                    MacroManager::get().remove(name);
                    this->refresh();
                }
            );
            delBtn->setPosition({w - 16.f - bw * 0.5f, rowH / 2});
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
