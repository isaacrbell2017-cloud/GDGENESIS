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

constexpr float kW = 380.f;
constexpr float kH = 236.f;
constexpr char const* kAnthropicURL = "https://api.anthropic.com/v1/messages";

constexpr ccColor3B kRed { 255, 95, 95 };
constexpr ccColor3B kGreen { 140, 255, 140 };
constexpr ccColor3B kYellow { 255, 230, 120 };

struct Chip {
    char const* label;
    char const* prompt;
};
constexpr Chip kChips[] = {
    { "Neon", "neon glow lights, magenta and cyan" },
    { "Space", "deep space with stars and a purple aura" },
    { "Nature", "lush forest with grass and bushes" },
    { "Castle", "dark dungeon with hanging chains and orange torch glow" },
    { "Lava", "hell inferno with embers, red and orange" },
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

} // namespace

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
    this->setID("generator-popup"_spr);

    this->buildFrame();

    // Header: sparkle + "Ask Claude Generator to Generate..."
    auto header = CCLabelBMFont::create("Ask Claude Generator to Generate...", "goldFont.fnt");
    header->limitLabelWidth(kW - 80.f, 0.62f, 0.3f);
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
    m_input = TextInput::create(kW - 60.f, "Type your prompt", "bigFont.fnt");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(160);
    m_input->setID("prompt-input"_spr);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Top, { 0.f, -64.f });

    auto hint = CCLabelBMFont::create("*You need to have a block selected for the decoration to work*", "chatFont.fnt");
    hint->setColor({ 255, 236, 170 });
    hint->limitLabelWidth(kW - 40.f, 0.65f, 0.3f);
    hint->setID("hint"_spr);
    m_mainLayer->addChildAtPosition(hint, Anchor::Top, { 0.f, -95.f });

    // Quick prompt chips
    auto chipMenu = CCMenu::create();
    chipMenu->setID("chip-menu"_spr);
    chipMenu->ignoreAnchorPointForPosition(false);
    chipMenu->setContentSize({ kW - 30.f, 28.f });
    for (int i = 0; i < static_cast<int>(std::size(kChips)); i++) {
        auto spr = ButtonSprite::create(kChips[i].label, "bigFont.fnt", "GJ_button_04.png", .8f);
        spr->setScale(.5f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GenesisPopup::onChip));
        btn->setTag(i);
        chipMenu->addChild(btn);
    }
    chipMenu->setLayout(RowLayout::create()->setGap(6.f));
    chipMenu->updateLayout();
    m_mainLayer->addChildAtPosition(chipMenu, Anchor::Top, { 0.f, -125.f });

    // Status line
    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setID("status"_spr);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Top, { 0.f, -154.f });

    // Buttons
    auto menu = CCMenu::create();
    menu->setID("main-menu"_spr);
    menu->ignoreAnchorPointForPosition(false);
    menu->setContentSize({ kW, kH });
    m_mainLayer->addChildAtPosition(menu, Anchor::Center);

    m_genSprite = ButtonSprite::create("Generate", "goldFont.fnt", "GJ_button_01.png", .9f);
    m_genBtn = CCMenuItemSpriteExtra::create(m_genSprite, this, menu_selector(GenesisPopup::onGenerate));
    m_genBtn->setID("generate-button"_spr);
    menu->addChildAtPosition(m_genBtn, Anchor::Bottom, { 0.f, 38.f });

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

std::string GenesisPopup::modeName() const {
    auto mode = Mod::get()->getSettingValue<std::string>("ai-mode");
    auto key = genesis::trim(Mod::get()->getSettingValue<std::string>("api-key"));
    if (mode == "Offline") return "Offline";
    if (key.empty()) return mode == "Claude API" ? "Claude (needs key)" : "Offline";
    return "Claude AI";
}

void GenesisPopup::setStatus(std::string const& text, ccColor3B color, bool hold) {
    if (!m_status) return;
    m_status->setString(text.c_str());
    m_status->setColor(color);
    m_status->limitLabelWidth(kW - 40.f, 0.42f, 0.2f);
    if (hold) m_holdTicks = 4;
}

void GenesisPopup::refreshStatus() {
    int n = genesis::selectedCount();
    if (n <= 0) {
        this->setStatus("No blocks selected - select one first!", kRed);
    }
    else {
        this->setStatus(fmt::format("{} object{} selected  |  {}", n, n == 1 ? "" : "s", this->modeName()), kGreen);
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
    this->setStatus("Claude is designing your deco" + std::string(m_dots, '.'), kYellow);
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

void GenesisPopup::onInfo(CCObject*) {
    genesis::sfx("counter003.ogg");
    auto mainCh = Mod::get()->getSettingValue<int64_t>("main-channel");
    auto accentCh = Mod::get()->getSettingValue<int64_t>("accent-channel");
    FLAlertLayer::create(
        "GdGenesis",
        fmt::format(
            "<cg>1.</c> Select the block(s) to decorate - a whole layout works too.\n"
            "<cg>2.</c> Type a prompt like <cy>neon glow</c>, <cy>space with stars</c> or <cy>dungeon chains</c>.\n"
            "<cg>3.</c> Press <cg>Generate</c>.\n\n"
            "Don't like it? Press the editor's <cy>Undo</c> and generate again.\n"
            "Deco colors use channels <cy>{}</c> and <cy>{}</c>.\n"
            "Add a Claude API key in settings for Claude-powered prompts. Without one, the instant offline generator is used.",
            mainCh, accentCh
        ),
        "OK"
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
    int selected = genesis::selectedCount();
    if (selected <= 0) {
        this->setStatus("Select a block first! The deco is built around it.", kRed, true);
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
        this->finish(genesis::planFromPrompt(prompt), "");
        return;
    }
    this->startAI(prompt, key, selected);
}

void GenesisPopup::startAI(std::string const& prompt, std::string const& key, int selected) {
    auto model = Mod::get()->getSettingValue<std::string>("model");
    auto timeout = std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("timeout"), 2, 30);

    auto message = matjson::makeObject({
        { "role", "user" },
        { "content", fmt::format("Prompt: {}\nSelection: {} objects.", prompt, selected) },
    });
    auto body = matjson::makeObject({
        { "model", model },
        { "max_tokens", 300 },
        { "system", genesis::aiSystemPrompt() },
        { "messages", matjson::Value(std::vector<matjson::Value> { message }) },
    });

    web::WebRequest req;
    req.header("x-api-key", key);
    req.header("anthropic-version", "2023-06-01");
    req.header("content-type", "application/json");
    req.bodyJSON(body);
    req.timeout(std::chrono::seconds(timeout));

    this->setBusy(true);
    m_task.spawn(req.post(kAnthropicURL), [this, prompt](web::WebResponse res) {
        DecoPlan plan;
        std::string note;
        std::string text;
        if (res.ok()) {
            auto json = res.json();
            if (json.isOk()) text = extractText(json.unwrap());
        }
        if (!text.empty() && genesis::planFromJson(text, plan)) {
            plan.fromAI = true;
        }
        else {
            plan = genesis::planFromPrompt(prompt);
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

void GenesisPopup::applyPlan(DecoPlan const& plan, std::string const& note) {
    this->setBusy(false);
    auto result = genesis::generate(LevelEditorLayer::get(), plan);
    if (!result.ok) {
        genesis::sfx("quitSound_01.ogg");
        this->setStatus(result.error, kRed, true);
        return;
    }

    genesis::sfx("achievement_01.ogg");
    Notification::create(
        fmt::format("{}: {} deco objects added{}", plan.name, result.created, plan.fromAI ? " by Claude" : ""),
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
