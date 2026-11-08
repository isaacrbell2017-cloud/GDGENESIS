#include "GenesisPopup.hpp"
#include "DecoEngine.hpp"
#include "Shared.hpp"

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <iterator>

using namespace geode::prelude;

bool GenesisPopup::s_open = false;

namespace {

constexpr float kW = 420.f;
constexpr float kH = 290.f;
constexpr char const* kAnthropicURL = "https://api.anthropic.com/v1/messages";

constexpr ccColor3B kRed { 255, 95, 95 };
constexpr ccColor3B kGreen { 140, 255, 140 };
constexpr ccColor3B kYellow { 255, 230, 120 };
constexpr ccColor3B kDim { 110, 110, 110 };

constexpr char const* kTierFrames[] = {
    "GJ_featuredCoin_001.png", "GJ_epicCoin_001.png", "GJ_epicCoin2_001.png", "GJ_epicCoin3_001.png"
};
constexpr ccColor3B kTierColors[] = {
    { 255, 230, 90 }, { 255, 150, 60 }, { 205, 130, 255 }, { 255, 100, 210 }
};

struct Chip {
    char const* label;
    char const* prompt;
};
constexpr Chip kChips[] = {
    { "Neon", "neon glow with inner glow fill, magenta and cyan" },
    { "Space", "deep space with stars, glow orbs and dots fill" },
    { "Nature", "lush forest with grass, bushes and checker fill" },
    { "Castle", "dark dungeon with hanging chains, tiles fill and orange glow" },
    { "Lava", "hell inferno with embers and glowing edges, red and orange" },
    { "Ice", "frozen ice with snow particles and crystal spires" },
};

std::string extractText(matjson::Value const& body) {
    if (!body.contains("content")) return "";
    auto content = body["content"];
    if (!content.isArray()) return "";
    for (size_t i = 0; i < content.size(); i++) {
        auto block = content[i];
        if (block.contains("text")) {
            auto text = block["text"].asString().unwrapOr("");
            if (!text.empty()) return text;
        }
    }
    return "";
}

std::string fillLabel(int fill) {
    if (fill <= 0) return "Fill: Auto";
    if (fill == 1) return "Fill: Off";
    return std::string("Fill: ") + fillStyleName(static_cast<FillStyle>(std::clamp(fill - 2, 0, kFillStyleCount - 1)));
}

struct EffortInfo {
    char const* api;   // value for output_config.effort, nullptr = model default
    int maxTokens;     // thinking counts toward max_tokens
    int autoTimeout;   // seconds
};

EffortInfo effortInfo(std::string const& effort) {
    if (effort == "Low") return { "low", 4096, 20 };
    if (effort == "Medium") return { "medium", 6000, 35 };
    if (effort == "High") return { "high", 10000, 60 };
    if (effort == "Extra High") return { "xhigh", 20000, 120 };
    if (effort == "Max") return { "max", 32000, 180 };
    return { nullptr, 6000, 45 };
}

std::string modelShortName(std::string const& model) {
    if (model.find("opus") != std::string::npos) return "Opus";
    if (model.find("sonnet") != std::string::npos) return "Sonnet";
    if (model.find("haiku") != std::string::npos) return "Haiku";
    return "Claude";
}

CCSprite* tierIcon(int t) {
    CCSprite* spr = CCSprite::createWithSpriteFrameName(kTierFrames[t]);
    if (!spr) spr = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
    if (spr) {
        float size = std::max(spr->getContentSize().width, spr->getContentSize().height);
        if (size > 0.f) spr->setScale(30.f / size);
    }
    return spr;
}

} // namespace

// ============================================================================
// Main popup
// ============================================================================

GenesisPopup* GenesisPopup::create(std::function<void()> onDone) {
    auto ret = new GenesisPopup();
    if (ret->init(std::move(onDone))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

GenesisPopup::~GenesisPopup() {
    s_open = false;
}

bool GenesisPopup::init(std::function<void()> onDone) {
    if (!Popup::init(kW, kH)) return false;
    s_open = true;
    m_onDone = std::move(onDone);
    m_choices = genesis::loadChoices();
    this->setID("generator-popup"_spr);

    this->buildFrame();

    // Header: sparkle + "Ask Claude Generator to Generate..."
    auto header = CCLabelBMFont::create("Ask Claude Generator to Generate...", "goldFont.fnt");
    header->limitLabelWidth(kW - 90.f, 0.62f, 0.3f);
    header->setID("header"_spr);
    m_mainLayer->addChildAtPosition(header, Anchor::Top, { 10.f, -24.f });

    CCSprite* spark = CCSprite::createWithSpriteFrameName("genesis_spark.png"_spr);
    if (!spark) spark = CCSprite::create("genesis_spark.png"_spr);
    if (spark) {
        float size = std::max(spark->getContentSize().width, spark->getContentSize().height);
        if (size > 0.f) spark->setScale(22.f / size);
        spark->setID("spark"_spr);
        float headerHalf = header->getScaledContentSize().width / 2.f;
        m_mainLayer->addChildAtPosition(spark, Anchor::Top, { 10.f - headerHalf - 16.f, -24.f });
    }

    // Prompt bar
    m_input = TextInput::create(kW - 50.f, "Type your prompt", "bigFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(200);
    m_input->setID("prompt-input"_spr);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Top, { 0.f, -56.f });

    auto hint = CCLabelBMFont::create("*You need to have a block selected for the decoration to work*", "chatFont.fnt");
    hint->setColor({ 255, 236, 170 });
    hint->limitLabelWidth(kW - 40.f, 0.6f, 0.3f);
    hint->setID("hint"_spr);
    m_mainLayer->addChildAtPosition(hint, Anchor::Top, { 0.f, -80.f });

    this->buildTierRow();
    this->buildOptionRow();

    // Quick prompt chips
    auto chipMenu = CCMenu::create();
    chipMenu->setID("chip-menu"_spr);
    chipMenu->ignoreAnchorPointForPosition(false);
    chipMenu->setContentSize({ kW - 30.f, 24.f });
    for (int i = 0; i < static_cast<int>(std::size(kChips)); i++) {
        auto spr = ButtonSprite::create(kChips[i].label, "bigFont.fnt", "GJ_button_04.png", .8f);
        spr->setScale(.42f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GenesisPopup::onChip));
        btn->setTag(i);
        chipMenu->addChild(btn);
    }
    chipMenu->setLayout(RowLayout::create()->setGap(5.f));
    chipMenu->updateLayout();
    m_mainLayer->addChildAtPosition(chipMenu, Anchor::Top, { 0.f, -186.f });

    // Status line
    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setID("status"_spr);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Top, { 0.f, -209.f });

    // Bottom buttons
    auto menu = CCMenu::create();
    menu->setID("main-menu"_spr);
    menu->ignoreAnchorPointForPosition(false);
    menu->setContentSize({ kW, kH });
    m_mainLayer->addChildAtPosition(menu, Anchor::Center);

    m_genSprite = ButtonSprite::create("Generate", "goldFont.fnt", "GJ_button_01.png", 1.f);
    m_genBtn = CCMenuItemSpriteExtra::create(m_genSprite, this, menu_selector(GenesisPopup::onGenerate));
    m_genBtn->setID("generate-button"_spr);
    menu->addChildAtPosition(m_genBtn, Anchor::Bottom, { 0.f, 32.f });

    auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    infoSpr->setScale(.75f);
    auto infoBtn = CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(GenesisPopup::onInfo));
    infoBtn->setID("info-button"_spr);
    menu->addChildAtPosition(infoBtn, Anchor::BottomLeft, { 26.f, 26.f });

    auto gearSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
    gearSpr->setScale(.55f);
    auto gearBtn = CCMenuItemSpriteExtra::create(gearSpr, this, menu_selector(GenesisPopup::onSettings));
    gearBtn->setID("settings-button"_spr);
    menu->addChildAtPosition(gearBtn, Anchor::BottomRight, { -28.f, 28.f });

    this->refreshTier();
    this->refreshOptions();
    this->refreshStatus();
    this->schedule(schedule_selector(GenesisPopup::tickStatus), 1.f);
    return true;
}

void GenesisPopup::buildFrame() {
    // The brown panel with the green/cyan frame and chains, straight from GD's own list menus.
    bool framed = false;
    if (auto list = CustomListView::create(CCArray::create(), BoomListType::Default, 1.f, 1.f)) {
        list->setVisible(false);
        if (auto frame = GJListLayer::create(list, "GdGenesis", { 191, 114, 62, 255 }, kW, kH, 0)) {
            frame->setPosition({ 0.f, 0.f });
            frame->setID("frame"_spr);
            m_mainLayer->addChild(frame, -1);
            framed = true;
        }
    }
    if (framed) {
        if (m_bgSprite) m_bgSprite->setVisible(false);
    }
    else {
        this->setTitle("GdGenesis");
    }
}

void GenesisPopup::buildTierRow() {
    auto tierMenu = CCMenu::create();
    tierMenu->setID("tier-menu"_spr);
    tierMenu->ignoreAnchorPointForPosition(false);
    tierMenu->setContentSize({ 240.f, 40.f });
    for (int t = 0; t < 4; t++) {
        auto icon = tierIcon(t);
        m_tierIcons[t] = icon;
        auto btn = CCMenuItemSpriteExtra::create(icon, this, menu_selector(GenesisPopup::onTier));
        btn->setTag(t);
        m_tierBtns[t] = btn;
        tierMenu->addChildAtPosition(btn, Anchor::Center, { -90.f + 60.f * t, 0.f });
    }
    m_mainLayer->addChildAtPosition(tierMenu, Anchor::Top, { -40.f, -110.f });

    auto quality = CCLabelBMFont::create("Quality", "goldFont.fnt");
    quality->setScale(0.45f);
    quality->setID("quality-label"_spr);
    m_mainLayer->addChildAtPosition(quality, Anchor::Top, { 130.f, -102.f });

    m_tierLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_tierLabel->setID("tier-label"_spr);
    m_mainLayer->addChildAtPosition(m_tierLabel, Anchor::Top, { 130.f, -120.f });
}

void GenesisPopup::buildOptionRow() {
    auto optMenu = CCMenu::create();
    optMenu->setID("option-menu"_spr);
    optMenu->ignoreAnchorPointForPosition(false);
    optMenu->setContentSize({ kW - 30.f, 30.f });

    m_selSprite = ButtonSprite::create("Selection", "bigFont.fnt", "GJ_button_01.png", .7f);
    m_selSprite->setScale(.55f);
    auto selBtn = CCMenuItemSpriteExtra::create(m_selSprite, this, menu_selector(GenesisPopup::onTarget));
    selBtn->setTag(0);
    selBtn->setID("selection-button"_spr);
    optMenu->addChild(selBtn);

    m_wholeSprite = ButtonSprite::create("Whole Level", "bigFont.fnt", "GJ_button_04.png", .7f);
    m_wholeSprite->setScale(.55f);
    auto wholeBtn = CCMenuItemSpriteExtra::create(m_wholeSprite, this, menu_selector(GenesisPopup::onTarget));
    wholeBtn->setTag(1);
    wholeBtn->setID("whole-level-button"_spr);
    optMenu->addChild(wholeBtn);

    m_fillSprite = ButtonSprite::create("Fill: Checker", "bigFont.fnt", "GJ_button_02.png", .7f);
    m_fillSprite->setScale(.55f);
    auto fillBtn = CCMenuItemSpriteExtra::create(m_fillSprite, this, menu_selector(GenesisPopup::onFill));
    fillBtn->setID("fill-button"_spr);
    optMenu->addChild(fillBtn);

    auto modSprite = ButtonSprite::create("Modules", "bigFont.fnt", "GJ_button_05.png", .7f);
    modSprite->setScale(.55f);
    auto modBtn = CCMenuItemSpriteExtra::create(modSprite, this, menu_selector(GenesisPopup::onModules));
    modBtn->setID("modules-button"_spr);
    optMenu->addChild(modBtn);

    optMenu->setLayout(RowLayout::create()->setGap(6.f));
    optMenu->updateLayout();
    m_mainLayer->addChildAtPosition(optMenu, Anchor::Top, { 0.f, -156.f });
}

void GenesisPopup::refreshTier() {
    int sel = static_cast<int>(m_choices.tier);
    for (int t = 0; t < 4; t++) {
        auto icon = typeinfo_cast<CCSprite*>(m_tierIcons[t]);
        if (!icon) continue;
        bool on = t == sel;
        icon->setColor(on ? ccColor3B { 255, 255, 255 } : kDim);
        icon->setOpacity(on ? 255 : 170);
        if (m_tierBtns[t]) m_tierBtns[t]->setScale(on ? 1.15f : 0.85f);
    }
    if (m_tierLabel) {
        m_tierLabel->setString(tierName(m_choices.tier));
        m_tierLabel->setColor(kTierColors[sel]);
        m_tierLabel->limitLabelWidth(110.f, 0.5f, 0.2f);
    }
}

void GenesisPopup::refreshOptions() {
    bool whole = m_choices.wholeLevel;
    if (m_selSprite) m_selSprite->updateBGImage(whole ? "GJ_button_04.png" : "GJ_button_01.png");
    if (m_wholeSprite) m_wholeSprite->updateBGImage(whole ? "GJ_button_01.png" : "GJ_button_04.png");
    if (m_fillSprite) m_fillSprite->setString(fillLabel(m_choices.fill).c_str());
}

bool GenesisPopup::wholeLevel() const {
    return m_choices.wholeLevel;
}

std::string GenesisPopup::modeName() const {
    auto mode = Mod::get()->getSettingValue<std::string>("ai-mode");
    auto key = genesis::trim(Mod::get()->getSettingValue<std::string>("api-key"));
    if (mode == "Offline") return "Offline";
    if (key.empty()) return mode == "Claude API" ? "Claude (needs key)" : "Offline";
    auto model = modelShortName(Mod::get()->getSettingValue<std::string>("model"));
    auto effort = Mod::get()->getSettingValue<std::string>("effort");
    return effort == "Default" ? "Claude " + model : fmt::format("Claude {} ({})", model, effort);
}

void GenesisPopup::setStatus(std::string const& text, ccColor3B color, bool hold) {
    if (!m_status) return;
    m_status->setString(text.c_str());
    m_status->setColor(color);
    m_status->limitLabelWidth(kW - 40.f, 0.4f, 0.2f);
    if (hold) m_holdTicks = 4;
}

void GenesisPopup::refreshStatus() {
    std::string tier = tierName(m_choices.tier);
    if (this->wholeLevel()) {
        int n = genesis::levelBlockCount();
        if (n <= 0) this->setStatus("Whole Level: no blocks in this level yet!", kRed);
        else this->setStatus(fmt::format("Whole Level: {} blocks  |  {}  |  {}", n, tier, this->modeName()), kGreen);
        return;
    }
    int n = genesis::selectedCount();
    if (n <= 0) {
        this->setStatus("No blocks selected - select one, or pick Whole Level!", kRed);
    }
    else {
        this->setStatus(fmt::format("{} object{} selected  |  {}  |  {}", n, n == 1 ? "" : "s", tier, this->modeName()), kGreen);
    }
}

void GenesisPopup::tickStatus(float) {
    if (m_busy) return;
    if (m_holdTicks > 0) {
        m_holdTicks--;
        return;
    }
    this->refreshStatus();
}

void GenesisPopup::tickDots(float) {
    m_dots = (m_dots + 1) % 4;
    m_busyTicks++;
    int seconds = static_cast<int>(m_busyTicks * 0.3f);
    this->setStatus(fmt::format("{} is designing your deco{}  ({}s)",
        this->modeName(), std::string(m_dots, '.'), seconds), kYellow);
}

void GenesisPopup::setBusy(bool busy) {
    m_busy = busy;
    if (m_genSprite) {
        m_genSprite->setString(busy ? "Thinking" : "Generate");
        m_genSprite->updateBGImage(busy ? "GJ_button_04.png" : "GJ_button_01.png");
    }
    if (m_genBtn) m_genBtn->setEnabled(!busy);
    if (busy) {
        m_dots = 0;
        m_busyTicks = 0;
        this->schedule(schedule_selector(GenesisPopup::tickDots), .3f);
    }
    else {
        this->unschedule(schedule_selector(GenesisPopup::tickDots));
    }
}

void GenesisPopup::onChip(CCObject* sender) {
    genesis::sfx("counter003.ogg");
    int i = sender->getTag();
    if (i < 0 || i >= static_cast<int>(std::size(kChips)) || !m_input) return;
    m_input->setString(kChips[i].prompt);
}

void GenesisPopup::onTier(CCObject* sender) {
    int t = std::clamp(sender->getTag(), 0, 3);
    genesis::sfx(t == 3 ? "achievement_01.ogg" : "counter003.ogg");
    m_choices.tier = static_cast<Tier>(t);
    genesis::saveChoices(m_choices);
    this->refreshTier();
    this->refreshStatus();
}

void GenesisPopup::onTarget(CCObject* sender) {
    genesis::sfx("counter003.ogg");
    m_choices.wholeLevel = sender->getTag() == 1;
    genesis::saveChoices(m_choices);
    this->refreshOptions();
    this->refreshStatus();
}

void GenesisPopup::onFill(CCObject*) {
    genesis::sfx("counter003.ogg");
    m_choices.fill = (m_choices.fill + 1) % (2 + kFillStyleCount);
    genesis::saveChoices(m_choices);
    this->refreshOptions();
    this->setStatus(
        m_choices.fill == 0 ? "Fill: Auto - the prompt and quality decide" :
        m_choices.fill == 1 ? "Fill: Off - nothing inside the blocks" :
        fmt::format("Fill: {} inside the blocks", fillStyleName(static_cast<FillStyle>(m_choices.fill - 2))),
        kYellow, true
    );
}

void GenesisPopup::onModules(CCObject*) {
    genesis::sfx("counter003.ogg");
    auto popup = GenesisModulesPopup::create([self = Ref<GenesisPopup>(this)]() {
        self->m_choices = genesis::loadChoices();
        self->refreshOptions();
        self->refreshStatus();
    });
    if (popup) popup->show();
}

void GenesisPopup::onInfo(CCObject*) {
    genesis::sfx("counter003.ogg");
    auto mainCh = Mod::get()->getSettingValue<int64_t>("main-channel");
    auto accentCh = Mod::get()->getSettingValue<int64_t>("accent-channel");
    auto fillCh = Mod::get()->getSettingValue<int64_t>("fill-channel");
    FLAlertLayer::create(
        nullptr,
        "GdGenesis",
        fmt::format(
            "<cg>1.</c> Select block(s), or tap <cy>Whole Level</c> to decorate your entire layout.\n"
            "<cg>2.</c> Pick a quality: <cy>Featured</c>, <co>Epic</c>, <cp>Legendary</c> or <cr>Mythic</c>. Higher = more detail, glow and color sections.\n"
            "<cg>3.</c> Type a prompt (like <cy>neon glow</c> or <cy>space with checker fill</c>) and press <cg>Generate</c>.\n\n"
            "<cy>Fill</c> decorates inside the blocks. <cy>Modules</c> lets you force any part on or off.\n"
            "Don't like it? Press the editor's Undo and generate again.\n"
            "Channels: main <cy>{}</c>, accent <cy>{}</c>, fill <cy>{}</c>.",
            mainCh, accentCh, fillCh
        ),
        "OK", nullptr, 400.f
    )->show();
}

void GenesisPopup::onSettings(CCObject*) {
    genesis::sfx("counter003.ogg");
    geode::openSettingsPopup(Mod::get());
}

void GenesisPopup::onGenerate(CCObject*) {
    if (m_busy) return;
    genesis::sfx("playSound_01.ogg");

    auto prompt = genesis::trim(std::string(m_input->getString()));
    if (prompt.empty()) {
        this->setStatus("Type a prompt first!", kRed, true);
        return;
    }
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_editorUI) {
        this->setStatus("Open the level editor first!", kRed, true);
        return;
    }

    auto promptPlan = genesis::planFromPrompt(prompt);
    bool whole = this->wholeLevel() || promptPlan.promptWholeLevel;
    int targets = whole ? genesis::levelBlockCount() : genesis::selectedCount();
    if (targets <= 0) {
        this->setStatus(whole ? "This level has no blocks yet - build a layout first!"
                              : "Select a block first, or pick Whole Level!", kRed, true);
        return;
    }

    auto mode = Mod::get()->getSettingValue<std::string>("ai-mode");
    auto key = genesis::trim(Mod::get()->getSettingValue<std::string>("api-key"));
    bool useAI = mode == "Claude API" || (mode == "Auto" && !key.empty());
    if (useAI && key.empty()) {
        this->setStatus("Add your Claude API key in settings (gear), or switch to Offline.", kRed, true);
        return;
    }
    if (!useAI) {
        this->finish(std::move(promptPlan), "");
        return;
    }
    this->startAI(prompt, key, targets);
}

void GenesisPopup::startAI(std::string const& prompt, std::string const& key, int targets) {
    auto model = Mod::get()->getSettingValue<std::string>("model");
    auto effort = effortInfo(Mod::get()->getSettingValue<std::string>("effort"));
    auto timeoutSetting = Mod::get()->getSettingValue<int64_t>("timeout");
    int64_t timeout = timeoutSetting > 0 ? std::clamp<int64_t>(timeoutSetting, 2, 300) : effort.autoTimeout;
    bool whole = this->wholeLevel();

    auto message = matjson::makeObject({
        { "role", "user" },
        { "content", fmt::format(
            "Prompt: {}\nTarget: {} ({} blocks)\nQuality: {}",
            prompt, whole ? "whole level" : "selection", targets, tierName(m_choices.tier)
        ) },
    });
    auto messages = matjson::Value(std::vector<matjson::Value> { message });
    auto body = effort.api
        ? matjson::makeObject({
            { "model", model },
            { "max_tokens", effort.maxTokens },
            { "system", genesis::aiSystemPrompt() },
            { "messages", messages },
            { "output_config", matjson::makeObject({ { "effort", std::string(effort.api) } }) },
        })
        : matjson::makeObject({
            { "model", model },
            { "max_tokens", effort.maxTokens },
            { "system", genesis::aiSystemPrompt() },
            { "messages", messages },
        });

    web::WebRequest req;
    req.header("x-api-key", key);
    req.header("anthropic-version", "2023-06-01");
    req.header("content-type", "application/json");
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(timeout));

    this->setBusy(true);
    m_task.spawn(req.post(kAnthropicURL), [this, prompt](web::WebResponse res) {
        auto promptPlan = genesis::planFromPrompt(prompt);
        DecoPlan plan;
        std::string note;
        std::string text;
        if (res.ok()) {
            auto json = res.json();
            if (json.isOk()) text = extractText(json.unwrap());
        }
        if (!text.empty() && genesis::planFromJson(text, plan)) {
            plan.fromAI = true;
            plan.promptTier = promptPlan.promptTier;
            plan.promptWholeLevel = promptPlan.promptWholeLevel;
        }
        else {
            plan = promptPlan;
            int code = res.code();
            if (res.ok()) note = "Claude's reply was unreadable - used the offline generator.";
            else if (code == 401) note = "Claude says the API key is invalid - used the offline generator.";
            else if (code <= 0) note = "Claude took too long - used the offline generator.";
            else note = fmt::format("Claude error {} - used the offline generator.", code);
            log::warn("GdGenesis AI fallback (HTTP {})", code);
        }
        this->finish(std::move(plan), std::move(note));
    });
}

void GenesisPopup::finish(DecoPlan plan, std::string note) {
    // Run on the next frame so the popup is never closed from inside its own callback.
    m_busy = true;
    geode::queueInMainThread([self = Ref<GenesisPopup>(this), plan = std::move(plan), note = std::move(note)]() {
        self->applyPlan(plan, note);
    });
}

void GenesisPopup::applyPlan(DecoPlan plan, std::string const& note) {
    this->setBusy(false);

    UserChoices choices = m_choices;
    if (plan.promptWholeLevel) choices.wholeLevel = true;
    genesis::finalizePlan(plan, choices);

    auto result = genesis::generate(LevelEditorLayer::get(), plan, choices.wholeLevel);
    if (!result.ok) {
        genesis::sfx("quitSound_01.ogg");
        this->setStatus(result.error, kRed, true);
        return;
    }

    genesis::sfx("achievement_01.ogg");
    Notification::create(
        fmt::format("{} ({}): {} deco objects added{}", plan.name, tierName(plan.tier), result.created,
            plan.fromAI ? " by Claude" : ""),
        NotificationIcon::Success, 2.5f
    )->show();
    if (!note.empty()) {
        Notification::create(note, NotificationIcon::Warning, 3.f)->show();
    }

    auto done = std::move(m_onDone);
    m_onDone = nullptr;
    m_silentClose = true;
    this->onClose(nullptr);
    if (done) done();
}

void GenesisPopup::onClose(CCObject* sender) {
    if (!m_silentClose) genesis::sfx("quitSound_01.ogg");
    m_task.cancel();
    this->unschedule(schedule_selector(GenesisPopup::tickDots));
    this->unschedule(schedule_selector(GenesisPopup::tickStatus));
    Popup::onClose(sender);
}

// ============================================================================
// Modules popup
// ============================================================================

namespace {
constexpr float kMW = 380.f;
constexpr float kMH = 250.f;
constexpr char const* kStateNames[] = { "Auto", "On", "Off" };
constexpr char const* kStateBGs[] = { "GJ_button_04.png", "GJ_button_01.png", "GJ_button_06.png" };
constexpr float kDensitySteps[] = { 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 2.5f, 3.f };
}

GenesisModulesPopup* GenesisModulesPopup::create(std::function<void()> onChanged) {
    auto ret = new GenesisModulesPopup();
    if (ret->init(std::move(onChanged))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool GenesisModulesPopup::init(std::function<void()> onChanged) {
    if (!Popup::init(kMW, kMH)) return false;
    m_onChanged = std::move(onChanged);
    m_choices = genesis::loadChoices();
    this->setTitle("Modules");
    this->setID("modules-popup"_spr);

    auto sub = CCLabelBMFont::create("Tap to cycle:  Auto (prompt decides)  /  On  /  Off", "chatFont.fnt");
    sub->setColor({ 255, 236, 170 });
    sub->limitLabelWidth(kMW - 40.f, 0.6f, 0.3f);
    m_mainLayer->addChildAtPosition(sub, Anchor::Top, { 0.f, -42.f });

    auto menu = CCMenu::create();
    menu->ignoreAnchorPointForPosition(false);
    menu->setContentSize({ kMW, kMH });
    menu->setID("modules-menu"_spr);
    m_mainLayer->addChildAtPosition(menu, Anchor::Center);

    for (int m = 0; m < kUserModuleCount; m++) {
        auto spr = ButtonSprite::create("BG Colors: Auto", "bigFont.fnt", "GJ_button_04.png", .6f);
        spr->setScale(.5f);
        m_btns[m] = spr;
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GenesisModulesPopup::onModule));
        btn->setTag(m);
        int col = m % 3;
        int row = m / 3;
        menu->addChildAtPosition(btn, Anchor::Top, { -118.f + 118.f * col, -70.f - 30.f * row });
    }

    // Density: < 1.00x >
    auto left = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    left->setScale(.45f);
    auto leftBtn = CCMenuItemSpriteExtra::create(left, this, menu_selector(GenesisModulesPopup::onDensity));
    leftBtn->setTag(-1);
    menu->addChildAtPosition(leftBtn, Anchor::Bottom, { -80.f, 64.f });

    auto right = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    right->setScale(.45f);
    right->setFlipX(true);
    auto rightBtn = CCMenuItemSpriteExtra::create(right, this, menu_selector(GenesisModulesPopup::onDensity));
    rightBtn->setTag(1);
    menu->addChildAtPosition(rightBtn, Anchor::Bottom, { 80.f, 64.f });

    m_densityLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_mainLayer->addChildAtPosition(m_densityLabel, Anchor::Bottom, { 0.f, 64.f });

    auto resetSpr = ButtonSprite::create("Reset", "goldFont.fnt", "GJ_button_06.png", .8f);
    resetSpr->setScale(.7f);
    auto resetBtn = CCMenuItemSpriteExtra::create(resetSpr, this, menu_selector(GenesisModulesPopup::onReset));
    menu->addChildAtPosition(resetBtn, Anchor::Bottom, { 0.f, 28.f });

    this->refresh();
    return true;
}

void GenesisModulesPopup::refresh() {
    for (int m = 0; m < kUserModuleCount; m++) {
        if (!m_btns[m]) continue;
        int state = std::clamp(m_choices.overrides[m], 0, 2);
        m_btns[m]->setString(fmt::format("{}: {}", moduleName(m), kStateNames[state]).c_str());
        m_btns[m]->updateBGImage(kStateBGs[state]);
    }
    if (m_densityLabel) {
        m_densityLabel->setString(fmt::format("Density: {:.2f}x", m_choices.density).c_str());
        m_densityLabel->setScale(0.45f);
    }
}

void GenesisModulesPopup::onModule(CCObject* sender) {
    genesis::sfx("counter003.ogg");
    int m = sender->getTag();
    if (m < 0 || m >= kUserModuleCount) return;
    m_choices.overrides[m] = (m_choices.overrides[m] + 1) % 3;
    genesis::saveChoices(m_choices);
    this->refresh();
}

void GenesisModulesPopup::onDensity(CCObject* sender) {
    genesis::sfx("counter003.ogg");
    int dir = sender->getTag() < 0 ? -1 : 1;
    int idx = 2;
    float best = 1e9f;
    for (int i = 0; i < static_cast<int>(std::size(kDensitySteps)); i++) {
        float d = std::abs(kDensitySteps[i] - m_choices.density);
        if (d < best) { best = d; idx = i; }
    }
    idx = std::clamp(idx + dir, 0, static_cast<int>(std::size(kDensitySteps)) - 1);
    m_choices.density = kDensitySteps[idx];
    genesis::saveChoices(m_choices);
    this->refresh();
}

void GenesisModulesPopup::onReset(CCObject*) {
    genesis::sfx("quitSound_01.ogg");
    m_choices.overrides = {};
    m_choices.density = 1.f;
    genesis::saveChoices(m_choices);
    this->refresh();
}

void GenesisModulesPopup::onClose(CCObject* sender) {
    genesis::saveChoices(m_choices);
    auto cb = std::move(m_onChanged);
    m_onChanged = nullptr;
    Popup::onClose(sender);
    if (cb) cb();
}
