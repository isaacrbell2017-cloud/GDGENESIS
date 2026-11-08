#include "Plan.hpp"
#include "Shared.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <vector>

using namespace geode::prelude;

char const* fillStyleName(FillStyle s) {
    switch (s) {
        case FillStyle::Glow: return "Glow";
        case FillStyle::Rim: return "Rim";
        case FillStyle::Dots: return "Dots";
        case FillStyle::Tiles: return "Tiles";
        case FillStyle::Checker: return "Checker";
        case FillStyle::Solid: return "Solid";
    }
    return "Glow";
}

char const* tierName(Tier t) {
    switch (t) {
        case Tier::Featured: return "Featured";
        case Tier::Epic: return "Epic";
        case Tier::Legendary: return "Legendary";
        case Tier::Mythic: return "Mythic";
    }
    return "Epic";
}

char const* moduleName(int m) {
    static constexpr std::array<char const*, M_Count> names = {
        "Lights", "Edge Glow", "Fill", "Aura", "Particles", "Chains",
        "Grass", "Bushes", "Clouds", "Spires", "BG Colors", "Custom"
    };
    return (m >= 0 && m < M_Count) ? names[m] : "?";
}

namespace {

// Module intensities in this order:
// Lights, Glow, Fill, Aura, Particles, Chains, Grass, Bushes, Clouds, Rods, Background
using ModRow = std::array<int, kUserModuleCount>;

struct Theme {
    std::vector<char const*> words;
    char const* name;
    ccColor3B main;
    ccColor3B accent;
    ModRow mods;
    FillStyle fill;
};

std::vector<Theme> const& themes() {
    static std::vector<Theme> list = {
        {{"neon", "glow", "glowy", "cyber", "cyberpunk", "synthwave", "modern", "electric", "disco", "club", "rgb"},
            "Neon Glow", {255, 0, 200}, {0, 230, 255}, {3, 2, 1, 2, 2, 0, 0, 0, 0, 0, 1}, FillStyle::Glow},
        {{"space", "galaxy", "cosmic", "stars", "night", "universe", "nebula", "planet", "alien", "moon"},
            "Deep Space", {120, 40, 255}, {255, 255, 255}, {1, 1, 1, 3, 3, 0, 0, 0, 0, 1, 1}, FillStyle::Dots},
        {{"nature", "forest", "jungle", "garden", "meadow", "spring", "tree", "trees", "leaf", "flower", "flowers"},
            "Forest", {60, 200, 80}, {220, 255, 120}, {0, 0, 0, 1, 1, 0, 3, 2, 1, 0, 1}, FillStyle::Checker},
        {{"sky", "heaven", "heavenly", "dream", "dreamy", "angel", "holy", "air", "fluffy"},
            "Sky Dream", {255, 255, 255}, {140, 210, 255}, {1, 1, 0, 1, 1, 0, 0, 0, 3, 0, 1}, FillStyle::Glow},
        {{"castle", "dungeon", "prison", "medieval", "knight", "fortress", "ruins", "temple"},
            "Dungeon", {110, 110, 125}, {255, 150, 40}, {1, 1, 1, 1, 1, 3, 0, 0, 0, 1, 1}, FillStyle::Tiles},
        {{"fire", "lava", "hell", "inferno", "volcano", "demon", "demonic", "magma", "burning", "flame", "flames", "nether"},
            "Inferno", {255, 50, 20}, {255, 170, 0}, {1, 2, 1, 2, 3, 1, 0, 0, 0, 0, 1}, FillStyle::Glow},
        {{"ice", "frost", "frozen", "snow", "snowy", "winter", "glacier", "cold", "arctic", "christmas"},
            "Frozen", {80, 220, 255}, {255, 255, 255}, {1, 2, 1, 1, 3, 0, 0, 0, 1, 2, 1}, FillStyle::Glow},
        {{"ocean", "water", "sea", "underwater", "aquatic", "wave", "waves", "deep", "reef"},
            "Ocean", {30, 110, 255}, {80, 255, 240}, {1, 1, 1, 2, 3, 0, 0, 0, 0, 0, 1}, FillStyle::Glow},
        {{"tech", "factory", "machine", "robot", "industrial", "mechanical", "circuit", "digital", "computer", "lab"},
            "Tech", {140, 150, 165}, {255, 220, 40}, {2, 1, 1, 1, 1, 1, 0, 0, 0, 2, 1}, FillStyle::Tiles},
        {{"toxic", "acid", "poison", "radioactive", "nuclear", "slime", "sludge", "venom"},
            "Toxic", {120, 255, 30}, {30, 160, 40}, {2, 2, 1, 2, 2, 0, 0, 0, 0, 0, 1}, FillStyle::Glow},
        {{"candy", "sweet", "cute", "pastel", "kawaii", "sugar", "bubblegum", "cotton", "cupcake"},
            "Candy", {255, 130, 200}, {255, 255, 255}, {1, 1, 1, 1, 2, 0, 0, 1, 1, 0, 1}, FillStyle::Checker},
        {{"spooky", "horror", "ghost", "haunted", "halloween", "creepy", "scary", "dark", "shadow", "evil"},
            "Haunted", {110, 40, 170}, {255, 120, 20}, {1, 1, 1, 2, 1, 2, 0, 0, 0, 0, 1}, FillStyle::Dots},
        {{"gold", "golden", "royal", "luxury", "king", "queen", "treasure", "rich", "palace"},
            "Royal Gold", {255, 200, 40}, {170, 60, 255}, {2, 1, 1, 1, 2, 0, 0, 0, 0, 2, 1}, FillStyle::Rim},
        {{"sunset", "sunrise", "dusk", "evening", "retro", "vapor", "vaporwave", "summer", "beach"},
            "Sunset", {255, 120, 40}, {255, 80, 170}, {1, 2, 1, 2, 1, 0, 0, 0, 2, 0, 1}, FillStyle::Glow},
    };
    return list;
}

struct NamedColor {
    char const* name;
    ccColor3B color;
};

std::vector<NamedColor> const& colorNames() {
    static std::vector<NamedColor> list = {
        {"red", {255, 40, 40}}, {"crimson", {200, 0, 40}}, {"scarlet", {255, 36, 0}},
        {"orange", {255, 140, 0}}, {"yellow", {255, 230, 40}}, {"amber", {255, 190, 0}},
        {"lime", {150, 255, 40}}, {"green", {40, 220, 80}}, {"emerald", {0, 200, 120}},
        {"mint", {120, 255, 190}}, {"teal", {0, 180, 170}}, {"turquoise", {60, 225, 210}},
        {"cyan", {0, 230, 255}}, {"aqua", {0, 255, 230}}, {"blue", {40, 110, 255}},
        {"navy", {20, 40, 140}}, {"indigo", {90, 50, 220}}, {"purple", {160, 60, 255}},
        {"violet", {190, 90, 255}}, {"lavender", {200, 170, 255}}, {"magenta", {255, 0, 200}},
        {"pink", {255, 110, 190}}, {"rose", {255, 80, 130}}, {"white", {255, 255, 255}},
        {"silver", {200, 210, 220}}, {"gray", {130, 130, 140}}, {"grey", {130, 130, 140}},
        {"black", {25, 25, 30}}, {"brown", {140, 85, 40}}, {"beige", {230, 210, 170}},
        {"gold", {255, 200, 40}}, {"golden", {255, 200, 40}},
    };
    return list;
}

std::array<std::vector<char const*>, M_Count> const& moduleWords() {
    static std::array<std::vector<char const*>, M_Count> words = {{
        /* Lights */ {"light", "lights", "led", "leds", "bulb", "bulbs", "lamp", "lamps", "lit"},
        /* Glow */ {"glow", "glowing", "glows", "outline", "outlines", "border", "borders", "edge", "edges", "neon", "shine", "shiny"},
        /* Fill */ {"fill", "filled", "filling", "inside", "interior", "inner", "infill"},
        /* Aura */ {"aura", "orb", "orbs", "bokeh", "depth", "haze", "fog", "foggy"},
        /* Particles */ {"particle", "particles", "dot", "sparkle", "sparkles", "sparkly", "star", "snowflakes", "ember", "embers",
            "firefly", "fireflies", "glitter", "dust", "bubble", "bubbles"},
        /* Chains */ {"chain", "chains", "hanging", "rope", "ropes"},
        /* Grass */ {"grass", "grassy", "tufts"},
        /* Bushes */ {"bush", "bushes", "shrub", "shrubs", "foliage", "leaves", "plant", "plants"},
        /* Clouds */ {"cloud", "clouds", "cloudy", "mist", "misty"},
        /* Rods */ {"rod", "rods", "spire", "spires", "antenna", "antennas", "pole", "poles", "crystal", "crystals", "pillar", "pillars"},
        /* Background */ {"background", "bg", "colors", "colours", "colored", "coloured"},
        /* Custom */ {"custom", "mine"},
    }};
    return words;
}

struct StyleWord {
    char const* word;
    FillStyle style;
};
constexpr StyleWord kStyleWords[] = {
    {"rim", FillStyle::Rim}, {"dots", FillStyle::Dots}, {"dotted", FillStyle::Dots}, {"led", FillStyle::Dots},
    {"leds", FillStyle::Dots}, {"tiles", FillStyle::Tiles}, {"tile", FillStyle::Tiles}, {"tiled", FillStyle::Tiles},
    {"grid", FillStyle::Tiles}, {"checker", FillStyle::Checker}, {"checkered", FillStyle::Checker},
    {"checkerboard", FillStyle::Checker}, {"pattern", FillStyle::Checker}, {"patterned", FillStyle::Checker},
    {"solid", FillStyle::Solid}, {"holes", FillStyle::Solid}, {"hole", FillStyle::Solid},
    {"gaps", FillStyle::Solid}, {"gap", FillStyle::Solid},
};

std::vector<std::string> tokenize(std::string const& text) {
    std::vector<std::string> out;
    std::string cur;
    for (unsigned char c : text) {
        if (std::isalnum(c)) {
            cur += static_cast<char>(std::tolower(c));
        }
        else if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool isNegation(std::string const& w) {
    return w == "no" || w == "without" || w == "not" || w == "zero" || w == "nothing";
}

bool contains(std::vector<char const*> const& list, std::string const& word) {
    for (auto w : list) {
        if (word == w) return true;
    }
    return false;
}

ccColor3B mix(ccColor3B a, ccColor3B b, float t) {
    auto lerp = [t](GLubyte x, GLubyte y) {
        return static_cast<GLubyte>(std::clamp(std::lround(x + (y - x) * t), 0L, 255L));
    };
    return { lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b) };
}

std::optional<ccColor3B> parseHex(std::string s) {
    s = genesis::trim(s);
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (s.size() != 6) return std::nullopt;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    int v[6];
    for (int i = 0; i < 6; i++) {
        v[i] = hex(s[i]);
        if (v[i] < 0) return std::nullopt;
    }
    return ccColor3B {
        static_cast<GLubyte>(v[0] * 16 + v[1]),
        static_cast<GLubyte>(v[2] * 16 + v[3]),
        static_cast<GLubyte>(v[4] * 16 + v[5]),
    };
}

std::optional<double> number(matjson::Value const& v) {
    if (auto d = v.asDouble(); d.isOk()) return d.unwrap();
    if (auto i = v.asInt(); i.isOk()) return static_cast<double>(i.unwrap());
    if (auto b = v.asBool(); b.isOk()) return b.unwrap() ? 1.0 : 0.0;
    return std::nullopt;
}

bool anyVisual(DecoPlan const& p) {
    for (int m = 0; m < M_Count; m++) {
        if (m != M_Background && p[m] > 0) return true;
    }
    return false;
}

} // namespace

namespace genesis {

DecoPlan planFromPrompt(std::string const& prompt) {
    auto tokens = tokenize(prompt);
    DecoPlan plan;
    plan.mods = {};
    ModRow defaults = {2, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0};
    for (int m = 0; m < kUserModuleCount; m++) plan[m] = defaults[m];

    // 1. Theme: the one with the most keyword hits wins.
    int best = -1;
    int bestHits = 0;
    auto const& list = themes();
    for (int t = 0; t < static_cast<int>(list.size()); t++) {
        int hits = 0;
        for (size_t i = 0; i < tokens.size(); i++) {
            if (i > 0 && isNegation(tokens[i - 1])) continue;
            if (contains(list[t].words, tokens[i])) hits++;
        }
        if (hits > bestHits) {
            bestHits = hits;
            best = t;
        }
    }
    if (best >= 0) {
        auto const& th = list[best];
        plan.name = th.name;
        plan.main = th.main;
        plan.accent = th.accent;
        for (int m = 0; m < kUserModuleCount; m++) plan[m] = th.mods[m];
        plan.fillStyle = th.fill;
    }

    // 2. Modules mentioned directly ("chains", "no grass", ...).
    std::array<int, M_Count> mentioned {};
    auto const& words = moduleWords();
    for (size_t i = 0; i < tokens.size(); i++) {
        bool negated = i > 0 && isNegation(tokens[i - 1]);
        for (int m = 0; m < M_Count; m++) {
            if (contains(words[m], tokens[i])) mentioned[m] = negated ? -1 : 1;
        }
    }

    // Fill styles ("checker", "tiles", "solid" ...) also switch Fill on.
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0 && isNegation(tokens[i - 1])) continue;
        for (auto const& sw : kStyleWords) {
            if (tokens[i] == sw.word) {
                plan.fillStyle = sw.style;
                mentioned[M_Fill] = 1;
            }
        }
    }
    // "fill in the blocks" / "fill the gaps" style phrases
    for (size_t i = 0; i + 1 < tokens.size(); i++) {
        if ((tokens[i] == "fill" || tokens[i] == "filling") && (tokens[i + 1] == "in" || tokens[i + 1] == "up")) {
            mentioned[M_Fill] = 1;
        }
    }

    bool anyOn = false;
    for (int m = 0; m < M_Count; m++) {
        if (m != M_Fill && m != M_Background && mentioned[m] == 1) anyOn = true;
    }
    if (best < 0 && anyOn) {
        // No theme, but specific things asked for: build only those.
        for (int m = 0; m < kUserModuleCount; m++) plan[m] = 0;
        plan.name = "Custom Deco";
    }
    for (int m = 0; m < M_Count; m++) {
        if (mentioned[m] == 1) plan[m] = std::max(plan[m], m == M_Background ? 1 : 2);
        if (mentioned[m] == -1) plan[m] = 0;
    }

    // 3. Density words.
    int density = 0;
    for (auto const& t : tokens) {
        if (t == "minimal" || t == "simple" || t == "clean" || t == "subtle" || t == "calm") density = -1;
        if (t == "dense" || t == "detailed" || t == "crazy" || t == "insane" || t == "lots" || t == "extreme" ||
            t == "busy" || t == "heavy" || t == "max" || t == "maximum" || t == "full" || t == "intense") density = 1;
    }
    for (int m = 0; m < M_Count; m++) {
        if (m != M_Background && plan[m] > 0) plan[m] = std::clamp(plan[m] + density, 1, 3);
    }

    // 4. Colors: first color word is the main color, second is the accent.
    std::vector<ccColor3B> found;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0 && isNegation(tokens[i - 1])) continue;
        for (auto const& c : colorNames()) {
            if (tokens[i] == c.name) {
                bool dupe = std::any_of(found.begin(), found.end(), [&](ccColor3B const& f) {
                    return f.r == c.color.r && f.g == c.color.g && f.b == c.color.b;
                });
                if (!dupe) found.push_back(c.color);
            }
        }
    }
    if (!found.empty()) {
        plan.main = found[0];
        plan.accent = found.size() > 1 ? found[1] : mix(found[0], { 255, 255, 255 }, 0.55f);
    }

    // 5. Size.
    for (auto const& t : tokens) {
        if (t == "big" || t == "huge" || t == "large" || t == "giant" || t == "massive") plan.scale = 1.35f;
        if (t == "small" || t == "tiny" || t == "mini") plan.scale = 0.75f;
    }

    // 6. Tier and whole-level words in the prompt.
    for (size_t i = 0; i < tokens.size(); i++) {
        auto const& t = tokens[i];
        if (t == "featured" || t == "feature") plan.promptTier = 0;
        if (t == "epic") plan.promptTier = 1;
        if (t == "legendary") plan.promptTier = 2;
        if (t == "mythic") plan.promptTier = 3;
        if ((t == "whole" || t == "entire" || t == "full") && i + 1 < tokens.size() &&
            (tokens[i + 1] == "level" || tokens[i + 1] == "layout")) {
            plan.promptWholeLevel = true;
        }
    }

    for (int m = 0; m < M_Count; m++) plan[m] = std::clamp(plan[m], 0, 3);
    if (!anyVisual(plan)) plan[M_Particles] = 1;
    plan.fromAI = false;
    return plan;
}

bool planFromJson(std::string const& text, DecoPlan& out) {
    auto a = text.find('{');
    auto b = text.rfind('}');
    if (a == std::string::npos || b == std::string::npos || b <= a) return false;

    auto parsed = matjson::parse(text.substr(a, b - a + 1));
    if (parsed.isErr()) return false;
    auto root = parsed.unwrap();
    if (!root.isObject()) return false;

    DecoPlan plan;
    plan.mods = {};
    matjson::Value mods = root;
    if (root.contains("modules") && root["modules"].isObject()) mods = root["modules"];

    static constexpr std::array<char const*, M_Count> keys = {
        "lights", "glow", "fill", "aura", "particles", "chains",
        "grass", "bushes", "clouds", "rods", "background", "custom"
    };
    int read = 0;
    for (int m = 0; m < M_Count; m++) {
        if (mods.contains(keys[m])) {
            if (auto n = number(mods[keys[m]])) {
                plan[m] = std::clamp(static_cast<int>(std::lround(*n)), 0, 3);
                read++;
            }
        }
    }
    if (read == 0) return false;

    if (root.contains("fillStyle")) {
        auto style = toLower(trim(root["fillStyle"].asString().unwrapOr("")));
        for (int s = 0; s < kFillStyleCount; s++) {
            if (style == toLower(fillStyleName(static_cast<FillStyle>(s)))) plan.fillStyle = static_cast<FillStyle>(s);
        }
    }

    matjson::Value colors = root;
    if (root.contains("colors") && root["colors"].isObject()) colors = root["colors"];
    if (colors.contains("main")) {
        if (auto c = parseHex(colors["main"].asString().unwrapOr(""))) plan.main = *c;
    }
    if (colors.contains("accent")) {
        if (auto c = parseHex(colors["accent"].asString().unwrapOr(""))) plan.accent = *c;
    }
    if (root.contains("scale")) {
        if (auto n = number(root["scale"])) plan.scale = std::clamp(static_cast<float>(*n), 0.5f, 1.8f);
    }
    if (root.contains("name")) {
        auto name = trim(root["name"].asString().unwrapOr(""));
        if (!name.empty()) plan.name = name.substr(0, 32);
    }

    if (!anyVisual(plan)) plan[M_Particles] = 1;
    plan.fromAI = true;
    out = plan;
    return true;
}

void finalizePlan(DecoPlan& plan, UserChoices const& choices) {
    Tier tier = plan.promptTier >= 0 ? static_cast<Tier>(plan.promptTier) : choices.tier;
    plan.tier = tier;
    bool whole = choices.wholeLevel;

    auto bump = [&](int m, int minValue) { plan[m] = std::max(plan[m], minValue); };
    auto boostActive = [&](int by) {
        for (int m = 0; m < M_Count; m++) {
            if (m != M_Background && plan[m] > 0) plan[m] = std::min(3, plan[m] + by);
        }
    };

    switch (tier) {
        case Tier::Featured:
            plan.density = 1.f;
            plan.sections = whole ? 2 : 1;
            break;
        case Tier::Epic:
            boostActive(1);
            bump(M_Glow, 1);
            bump(M_Aura, 1);
            bump(M_Particles, 1);
            plan.density = 1.2f;
            plan.sections = whole ? 3 : 1;
            break;
        case Tier::Legendary:
            boostActive(1);
            bump(M_Lights, 1);
            bump(M_Glow, 2);
            bump(M_Fill, 2);
            bump(M_Aura, 2);
            bump(M_Particles, 2);
            bump(M_Background, 1);
            plan.halos = true;
            plan.density = 1.45f;
            plan.sections = whole ? 4 : 1;
            break;
        case Tier::Mythic:
            boostActive(2);
            bump(M_Lights, 2);
            bump(M_Glow, 3);
            bump(M_Fill, 3);
            bump(M_Aura, 3);
            bump(M_Particles, 3);
            bump(M_Background, 1);
            plan.halos = true;
            plan.sparkles = true;
            plan.density = 1.75f;
            plan.sections = whole ? 6 : 1;
            break;
    }

    // Fill choice from the menu.
    if (choices.fill == 1) {
        plan[M_Fill] = 0;
    }
    else if (choices.fill >= 2) {
        plan.fillStyle = static_cast<FillStyle>(std::clamp(choices.fill - 2, 0, kFillStyleCount - 1));
        bump(M_Fill, 2);
    }

    // Module overrides from the Modules page.
    for (int m = 0; m < kUserModuleCount; m++) {
        if (choices.overrides[m] == 1) bump(m, m == M_Background ? 1 : 2);
        if (choices.overrides[m] == 2) plan[m] = 0;
    }

    plan.density *= std::clamp(choices.density, 0.25f, 3.f);
    for (int m = 0; m < M_Count; m++) plan[m] = std::clamp(plan[m], 0, 3);
    if (!anyVisual(plan)) plan[M_Particles] = 1;
}

std::string aiSystemPrompt() {
    return
        "You are GdGenesis, a Geometry Dash 2.2 decoration designer inside the level editor. "
        "The user selected blocks (or their whole level) and typed a decoration prompt. Choose a decoration recipe.\n"
        "Reply with ONLY one minified JSON object and nothing else, in this exact shape:\n"
        "{\"name\":\"short theme name\",\"main\":\"#RRGGBB\",\"accent\":\"#RRGGBB\",\"scale\":1.0,"
        "\"lights\":0,\"glow\":0,\"fill\":0,\"fillStyle\":\"Glow\",\"aura\":0,\"particles\":0,\"chains\":0,"
        "\"grass\":0,\"bushes\":0,\"clouds\":0,\"rods\":0,\"background\":0,\"custom\":0}\n"
        "Each module is an intensity from 0 (off) to 3 (max):\n"
        "lights = small light dots along the block edges\n"
        "glow = soft glow pieces along the outside of block edges (classic featured glow look)\n"
        "fill = decoration inside the blocks; fillStyle is one of Glow (inner edge glow), Rim (dots just inside edges), "
        "Dots (LED dot grid), Tiles (outline tile per block), Checker (tinted checkerboard), Solid (fill enclosed holes)\n"
        "aura = big soft glow orbs behind the blocks for depth\n"
        "particles = small floating dots and glow specks (stars, snow, embers, bubbles)\n"
        "chains = chains hanging under platforms\n"
        "grass = grass tufts on top edges\n"
        "bushes = bushes peeking over top edges\n"
        "clouds = clouds above the build\n"
        "rods = small spires on top edges\n"
        "background = 1 to also recolor the background and ground to match\n"
        "custom = the user's own deco objects (only when the prompt says custom or my objects)\n"
        "scale is 0.6 to 1.6. main is the dominant deco color and accent the highlight color; match them to the prompt. "
        "Use 3 to 6 modules and aim for featured-quality deco that keeps the gameplay readable.";
}

UserChoices loadChoices() {
    auto mod = Mod::get();
    UserChoices c;
    c.tier = static_cast<Tier>(std::clamp<int64_t>(mod->getSavedValue<int64_t>("tier", 1), 0, 3));
    c.wholeLevel = mod->getSavedValue<bool>("whole-level", false);
    c.fill = static_cast<int>(std::clamp<int64_t>(mod->getSavedValue<int64_t>("fill", 0), 0, 1 + kFillStyleCount));
    for (int m = 0; m < kUserModuleCount; m++) {
        c.overrides[m] = static_cast<int>(std::clamp<int64_t>(
            mod->getSavedValue<int64_t>(fmt::format("module-{}", m), 0), 0, 2));
    }
    c.density = static_cast<float>(std::clamp(mod->getSavedValue<double>("density", 1.0), 0.25, 3.0));
    return c;
}

void saveChoices(UserChoices const& c) {
    auto mod = Mod::get();
    mod->setSavedValue<int64_t>("tier", static_cast<int64_t>(c.tier));
    mod->setSavedValue<bool>("whole-level", c.wholeLevel);
    mod->setSavedValue<int64_t>("fill", c.fill);
    for (int m = 0; m < kUserModuleCount; m++) {
        mod->setSavedValue<int64_t>(fmt::format("module-{}", m), c.overrides[m]);
    }
    mod->setSavedValue<double>("density", c.density);
}

} // namespace genesis
