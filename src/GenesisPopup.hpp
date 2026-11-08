#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
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
    void onGenerate(cocos2d::CCObject*);
    void onChip(cocos2d::CCObject* sender);
    void onInfo(cocos2d::CCObject*);
    void onSettings(cocos2d::CCObject*);

    void startAI(std::string const& prompt, std::string const& key, int selected);
    void finish(DecoPlan plan, std::string note);
    void applyPlan(DecoPlan const& plan, std::string const& note);

    void setStatus(std::string const& text, cocos2d::ccColor3B color, bool hold = false);
    void refreshStatus();
    void setBusy(bool busy);
    void tickDots(float);
    void tickStatus(float);
    std::string modeName() const;

private:
    static bool s_open;

    geode::TextInput* m_input = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    ButtonSprite* m_genSprite = nullptr;
    CCMenuItemSpriteExtra* m_genBtn = nullptr;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
    std::function<void()> m_onDone;
    bool m_busy = false;
    bool m_silentClose = false;
    int m_dots = 0;
    int m_holdTicks = 0;
};
