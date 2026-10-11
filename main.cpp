#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include "Render.hpp"
#include "Studio.hpp"

using namespace geode::prelude;

// ============================================================
// Rendering Utils
// ============================================================

// Kunci waktu game saat merender: tiap langkah = tepat 1/FPS detik,
// lalu ambil 1 frame setelah game berjalan.
class $modify(KBScheduler, CCScheduler) {
    void update(float dt) {
        auto& r = RenderManager::get();
        if (r.rendering) dt = 1.f / static_cast<float>(RenderManager::fps());
        CCScheduler::update(dt);
        if (r.rendering && r.ticked && PlayLayer::get()) {
            r.ticked = false;
            r.captureFrame();
        }
    }
};

class $modify(KBBaseGameLayer, GJBaseGameLayer) {
    void kbDrawHitboxes() {
        CCNode* layer = m_objectLayer;
        if (!layer) return;
        bool on = RenderManager::hitboxes();
        auto* node = static_cast<CCDrawNode*>(layer->getChildByTag(7772));
        if (!node) {
            if (!on) return;
            node = CCDrawNode::create();
            node->setTag(7772);
            layer->addChild(node, 5000);
        }
        if (!on) {
            if (node->isVisible()) {
                node->clear();
                node->setVisible(false);
            }
            return;
        }
        node->setVisible(true);
        node->clear();

        auto drawRectBox = [node](CCRect const& r, ccColor4F fill, ccColor4F border) {
            CCPoint pts[4] = {
                CCPoint{r.getMinX(), r.getMinY()}, CCPoint{r.getMaxX(), r.getMinY()},
                CCPoint{r.getMaxX(), r.getMaxY()}, CCPoint{r.getMinX(), r.getMaxY()}
            };
            node->drawPolygon(pts, 4, fill, 1.f, border);
        };

        CCPoint ref = m_player1 ? m_player1->getPosition() : CCPoint{0.f, 0.f};
        if (m_objects) {
            for (auto* obj : CCArrayExt<GameObject*>(m_objects)) {
                if (!obj) continue;
                auto type = obj->m_objectType;
                if (type == GameObjectType::Decoration) continue;
                if (obj->m_isDecoration || obj->m_isDecoration2) continue;
                CCPoint op = obj->getPosition();
                if (std::fabs(op.x - ref.x) > 520.f || std::fabs(op.y - ref.y) > 420.f) continue;

                ccColor4F border = {0.f, 1.f, 0.35f, 0.95f};
                if (type == GameObjectType::Hazard) border = {1.f, 0.1f, 0.1f, 0.95f};
                else if (type == GameObjectType::Solid || type == GameObjectType::Slope) border = {0.2f, 0.5f, 1.f, 0.95f};
                ccColor4F fill = {border.r, border.g, border.b, 0.12f};
                drawRectBox(obj->getObjectRect(), fill, border);
            }
        }
        if (m_player1) drawRectBox(m_player1->getObjectRect(), ccColor4F{1.f, 1.f, 0.f, 0.15f}, ccColor4F{1.f, 1.f, 0.f, 1.f});
        if (m_player2 && m_gameState.m_isDualMode) {
            drawRectBox(m_player2->getObjectRect(), ccColor4F{1.f, 1.f, 0.f, 0.15f}, ccColor4F{1.f, 1.f, 0.f, 1.f});
        }
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);
        if (PlayLayer::get()) {
            auto& r = RenderManager::get();
            r.ticked = true;
            this->kbDrawHitboxes();
            r.updateHud();
        }
    }
};

class $modify(KBPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (m_uiLayer) {
            auto win = CCDirector::get()->getWinSize();
            auto hud = CCNode::create();
            hud->setTag(RenderManager::HUD_TAG);
            hud->setPosition({win.width / 2.f, 16.f});

            auto bg = NineSlice::create("square02_001.png");
            bg->setContentSize({150.f, 20.f});
            bg->setColor({12, 14, 24});
            bg->setOpacity(200);
            bg->setPosition({0.f, 0.f});
            hud->addChild(bg);

            auto dot = CCDrawNode::create();
            dot->drawDot({-62.f, 0.f}, 4.f, ccColor4F{1.f, 0.32f, 0.38f, 1.f});
            hud->addChild(dot);

            auto label = CCLabelBMFont::create("REC 00:00", "chatFont.fnt");
            label->setScale(0.6f);
            label->setColor({255, 255, 255});
            label->setPosition({8.f, 0.f});
            label->setTag(1);
            hud->addChild(label);

            hud->setVisible(false);
            m_uiLayer->addChild(hud, 150);
        }
        return true;
    }

    void levelComplete() {
        auto& r = RenderManager::get();
        if (r.rendering && RenderManager::autoStop() && r.endCountdown < 0) {
            r.endCountdown = RenderManager::fps() * 3;
        }
        PlayLayer::levelComplete();
    }

    void onQuit() {
        RenderManager::get().stop(true);
        PlayLayer::onQuit();
    }
};

class $modify(KBPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        bool rec = RenderManager::get().rendering;
        auto bg = NineSlice::create("square02_001.png");
        bg->setContentSize({84.f, 26.f});
        bg->setColor(rec ? ccColor3B{255, 82, 96} : ccColor3B{12, 14, 24});
        bg->setOpacity(rec ? 255 : 220);

        auto holder = CCNode::create();
        holder->setContentSize({84.f, 26.f});
        bg->setPosition({42.f, 13.f});
        holder->addChild(bg);

        if (!rec) {
            auto dot = CCDrawNode::create();
            dot->drawDot({14.f, 13.f}, 4.f, ccColor4F{1.f, 0.32f, 0.38f, 1.f});
            holder->addChild(dot);
        }
        auto lbl = CCLabelBMFont::create(rec ? "STOP REC" : "Render", "bigFont.fnt");
        lbl->limitLabelWidth(rec ? 70.f : 52.f, 0.45f, 0.1f);
        lbl->setPosition({rec ? 42.f : 52.f, 13.f});
        holder->addChild(lbl);

        auto item = CCMenuItemExt::createSpriteExtra(holder, [this](CCMenuItemSpriteExtra*) {
            RenderStudio::create(this)->show();
        });
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        item->setPosition({56.f, 30.f});
        menu->addChild(item);
        this->addChild(menu, 100);
    }
};
