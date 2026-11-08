#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/modify/EditorUI.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "GenesisPopup.hpp"
#include "Shared.hpp"

using namespace geode::prelude;

namespace {

// Rects (in world space) of buttons and labels already on a layer.
void collectTaken(CCNode* node, std::vector<CCRect>& out, CCSize const& win) {
    if (!node || !node->isVisible()) return;
    auto children = node->getChildren();
    if (!children) return;
    for (auto child : CCArrayExt<CCNode*>(children)) {
        if (!child || !child->isVisible()) continue;
        if (typeinfo_cast<CCMenuItem*>(child) || typeinfo_cast<CCLabelBMFont*>(child)) {
            auto bb = child->boundingBox();
            auto bl = node->convertToWorldSpace(bb.origin);
            auto tr = node->convertToWorldSpace(CCPoint(bb.getMaxX(), bb.getMaxY()));
            CCRect rect(std::min(bl.x, tr.x), std::min(bl.y, tr.y), std::abs(tr.x - bl.x), std::abs(tr.y - bl.y));
            if (rect.size.width < win.width * 0.6f && rect.size.height < win.height * 0.6f) out.push_back(rect);
        }
        collectTaken(child, out, win);
    }
}

// First free corner/edge spot on the pause menu so the button never covers anything.
CCPoint findFreeSpot(CCNode* layer) {
    auto win = CCDirector::sharedDirector()->getWinSize();
    std::vector<CCRect> taken;
    collectTaken(layer, taken, win);

    std::vector<CCPoint> candidates = {
        { 38.f, win.height - 40.f },
        { win.width / 2.f, win.height - 34.f },
        { 38.f, 46.f },
        { win.width - 38.f, 46.f },
        { win.width / 2.f, 34.f },
        { 38.f, win.height / 2.f },
        { win.width - 38.f, win.height / 2.f },
        { win.width - 38.f, win.height - 90.f },
    };
    for (auto const& c : candidates) {
        CCRect area(c.x - 26.f, c.y - 36.f, 52.f, 64.f); // button + label underneath
        bool clear = true;
        for (auto const& t : taken) {
            if (area.intersectsRect(t)) {
                clear = false;
                break;
            }
        }
        if (clear) return c;
    }
    return candidates.front();
}

} // namespace

// The pause menu (the "||" button in the editor) gets the GdGenesis button.
class $modify(GenesisPauseLayer, EditorPauseLayer) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorPauseLayer::init(lel)) return false;

        // With Key Activation on, the key replaces this button.
        if (Mod::get()->getSettingValue<bool>("key-activation")) return true;

        CCSprite* spark = CCSprite::createWithSpriteFrameName("genesis_spark.png"_spr);
        if (!spark) spark = CCSprite::create("genesis_spark.png"_spr);
        if (!spark) spark = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
        float size = std::max(spark->getContentSize().width, spark->getContentSize().height);
        if (size > 0.f) spark->setScale(26.f / size);

        auto circle = CircleButtonSprite::create(spark, CircleBaseColor::Green, CircleBaseSize::Medium);
        CCNode* look = circle ? static_cast<CCNode*>(circle) : static_cast<CCNode*>(spark);
        look->setScale(look->getScale() * 0.85f);

        auto btn = CCMenuItemSpriteExtra::create(look, this, menu_selector(GenesisPauseLayer::onGenesis));
        btn->setID("open-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("menu"_spr);
        menu->addChild(btn);
        btn->setPosition({ 0.f, 0.f });

        auto label = CCLabelBMFont::create("GdGenesis", "goldFont.fnt");
        label->setScale(0.38f);
        label->setPosition({ 0.f, -28.f });
        label->setID("label"_spr);
        menu->addChild(label);

        menu->setPosition(findFreeSpot(this));
        this->addChild(menu, 50);
        return true;
    }

    void onGenesis(CCObject*) {
        genesis::sfx("counter003.ogg");
        genesis::openGenerator([self = Ref<EditorPauseLayer>(this)]() {
            // Close the pause menu so you see the new deco right away.
            if (self->getParent()) self->onResume(nullptr);
        });
    }
};

// Key Activation: open GdGenesis straight from the editor.
class $modify(GenesisEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "activation-key"),
            [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
                if (!down || repeat) return;
                if (!Mod::get()->getSettingValue<bool>("key-activation")) return;
                genesis::sfx("counter003.ogg");
                genesis::openGenerator(nullptr);
            }
        );
        return true;
    }
};
