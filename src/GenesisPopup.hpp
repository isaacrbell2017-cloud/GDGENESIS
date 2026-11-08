#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <array>
#include <functional>
#include <string>
#include "Plan.hpp"

class GenesisPopup : public geode::Popup {
public:
    static GenesisPopup* create(std::function<void()> onDone);
    static bool isOpen() { return s_open; }
    ~GenesisPopup() override;

protected:
    bool init(std::function<void()> onDone);
    void onClose(cocos2d::CCObject* sender) override;

    void buildFrame();
    void buildTierRow();
    void buildOptionRow();
    void refreshTier();
    void refreshOptions();

    void onGenerate(cocos2d::CCObject*);
    void onChip(cocos2d::CCObject* sender);
    void onTier(cocos2d::CCObject* sender);
    void onTarget(cocos2d::CCObject* sender);
    void onFill(cocos2d::CCObject*);
    void onModules(cocos2d::CCObject*);
    void onInfo(cocos2d::CCObject*);
    void onSettings(cocos2d::CCObject*);

    void startAI(std::string const& prompt, std::string const& key, int targets);
    void finish(DecoPlan plan, std::string note);
    void applyPlan(DecoPlan plan, std::string const& note);

    void setStatus(std::string const& text, cocos2d::ccColor3B color, bool hold = false);
    void refreshStatus();
    void setBusy(bool busy);
    void tickDots(float);
    void tickStatus(float);
    std::string modeName() const;
    bool wholeLevel() const;

private:
    static bool s_open;

    UserChoices m_choices;
    geode::TextInput* m_input = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_tierLabel = nullptr;
    std::array<CCMenuItemSpriteExtra*, 4> m_tierBtns {};
    std::array<cocos2d::CCNode*, 4> m_tierIcons {};
    ButtonSprite* m_selSprite = nullptr;
    ButtonSprite* m_wholeSprite = nullptr;
    ButtonSprite* m_fillSprite = nullptr;
    ButtonSprite* m_genSprite = nullptr;
    CCMenuItemSpriteExtra* m_genBtn = nullptr;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
    std::function<void()> m_onDone;
    bool m_busy = false;
    bool m_silentClose = false;
    int m_dots = 0;
    int m_busyTicks = 0;
    int m_holdTicks = 0;
};

// The Modules page: force each module Auto / On / Off and set density.
class GenesisModulesPopup : public geode::Popup {
public:
    static GenesisModulesPopup* create(std::function<void()> onChanged);

protected:
    bool init(std::function<void()> onChanged);
    void onModule(cocos2d::CCObject* sender);
    void onDensity(cocos2d::CCObject* sender);
    void onReset(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject* sender) override;
    void refresh();

private:
    UserChoices m_choices;
    std::array<ButtonSprite*, kUserModuleCount> m_btns {};
    cocos2d::CCLabelBMFont* m_densityLabel = nullptr;
    std::function<void()> m_onChanged;
};
