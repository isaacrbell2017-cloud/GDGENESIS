#include "Plan.hpp"
#include "Shared.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <vector>

using namespace geode::prelude;

namespace {

struct Theme {
    std::vector<char const*> words;
    char const* name;
    ccColor3B main;
    ccColor3B accent;
    // lights, aura, particles, chains, grass, bushes, clouds, rods
    std::array<int, 8> mods;
};

std::vector<Theme> const& themes() {
    static std::vector<Theme> list = {
        {{"neon", "glow", "glowy", "cyber", "cyberpunk", "synthwave", "modern", "electric", "disco", "club", "rgb"},
            "Neon Glow", {255, 0, 200}, {0, 230, 255}, {3, 2, 2, 0, 0, 0, 0, 0}},
        {{"space", "galaxy", "cosmic", "stars", "night", "universe", "nebula", "planet", "alien", "moon"},
            "Deep Space", {120, 40, 255}, {255, 255, 255}, {1, 3, 3, 0, 0, 0, 0, 1}},
        {{"nature", "forest", "jungle", "garden", "meadow", "spring", "tree", "trees", "leaf", "flower", "flowers"},
            "Forest", {60, 200, 80}, {220, 255, 120}, {0, 1, 1, 0, 3, 2, 1, 0}},
        {{"sky", "heaven", "heavenly", "dream", "dreamy", "angel", "holy", "air", "fluffy"},
            "Sky Dream", {255, 255, 255}, {140, 210, 255}, {1, 1, 1, 0, 0, 0, 3, 0}},
        {{"castle", "dungeon", "prison", "medieval", "knight", "fortress", "ruins", "temple"},
            "Dungeon", {110, 110, 125}, {255, 150, 40}, {1, 1, 1, 3, 0, 0, 0, 1}},
        {{"fire", "lava", "hell", "inferno", "volcano", "demon", "demonic", "magma", "burning", "flame", "flames", "nether"},
            "Inferno", {255, 50, 20}, {255, 170, 0}, {1, 2, 3, 1, 0, 0, 0, 0}},
        {{"ice", "frost", "frozen", "snow", "snowy", "winter", "glacier", "cold", "arctic", "christmas"},
            "Frozen", {80, 220, 255}, {255, 255, 255}, {1, 1, 3, 0, 0, 0, 1, 2}},
        {{"ocean", "water", "sea", "underwater", "aquatic", "wave", "waves", "deep", "reef"},
            "Ocean", {30, 110, 255}, {80, 255, 240}, {1, 2, 3, 0, 0, 0, 0, 0}},
        {{"tech", "factory", "machine", "robot", "industrial", "mechanical", "circuit", "digital", "computer", "lab"},
            "Tech", {140, 150, 165}, {255, 220, 40}, {2, 1, 1, 1, 0, 0, 0, 2}},
        {{"toxic", "acid", "poison", "radioactive", "nuclear", "slime", "sludge", "venom"},
            "Toxic", {120, 255, 30}, {30, 160, 40}, {2, 2, 2, 0, 0, 0, 0, 0}},
        {{"candy", "sweet", "cute", "pastel", "kawaii", "sugar", "bubblegum", "cotton", "cupcake"},
            "Candy", {255, 130, 200}, {255, 255, 255}, {1, 1, 2, 0, 0, 1, 1, 0}},
        {{"spooky", "horror", "ghost", "haunted", "halloween", "creepy", "scary", "dark", "shadow", "evil"},
            "Haunted", {110, 40, 170}, {255, 120, 20}, {1, 2, 1, 2, 0, 0, 0, 0}},
        {{"gold", "golden", "royal", "luxury", "king", "queen", "treasure", "rich", "palace"},
            "Royal Gold", {255, 200, 40}, {170, 60, 255}, {2, 1, 2, 0, 0, 0, 0, 2}},
        {{"sunset", "sunrise", "dusk", "evening", "retro", "vapor", "vaporwave", "summer", "beach"},
            "Sunset", {255, 120, 40}, {255, 80, 170}, {1, 2, 1, 0, 0, 0, 2, 0}},
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

enum Module { Lights, Aura, Particles, Chains, Grass, Bushes, Clouds, Rods, Custom, ModuleCount };

std::array<std::vector<char const*>, ModuleCount> const& moduleWords() {
    static std::array<std::vector<char const*>, ModuleCount> words = {{
        {"light", "lights", "led", "leds", "bulb", "bulbs", "outline", "outlines", "border", "borders", "edge", "edges", "lamp", "lamps", "lit"},
        {"aura", "orb", "orbs", "bokeh", "background", "bg", "depth", "haze", "fog", "foggy"},
        {"particle", "particles", "dot", "dots", "sparkle", "sparkles", "sparkly", "star", "snowflakes", "ember", "embers",
            "firefly", "fireflies", "glitter", "dust", "bubble", "bubbles"},
        {"chain", "chains", "hanging", "rope", "ropes"},
        {"grass", "grassy", "tufts"},
        {"bush", "bushes", "shrub", "shrubs", "foliage", "leaves", "plant", "plants"},
        {"cloud", "clouds", "cloudy", "mist", "misty"},
        {"rod", "rods", "spire", "spires", "antenna", "antennas", "pole", "poles", "crystal", "crystals", "pillar", "pillars"},
        {"custom", "mine"},
    }};
    return words;
}

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

int* moduleField(DecoPlan& p, int m) {
    switch (m) {
        case Lights: return &p.lights;
        case Aura: return &p.aura;
        case Particles: return &p.particles;
        case Chains: return &p.chains;
        case Grass: return &p.grass;
        case Bushes: return &p.bushes;
        case Clouds: return &p.clouds;
        case Rods: return &p.rods;
        default: return &p.custom;
    }
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
    return std::nullopt;
}

} // namespace

namespace genesis {

DecoPlan planFromPrompt(std::string const& prompt) {
    auto tokens = tokenize(prompt);
    DecoPlan plan;

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
        for (int m = 0; m < 8; m++) *moduleField(plan, m) = th.mods[m];
        plan.custom = 0;
    }

    // 2. Modules mentioned directly ("chains", "no grass", ...).
    std::array<int, ModuleCount> mentioned {};
    auto const& words = moduleWords();
    for (size_t i = 0; i < tokens.size(); i++) {
        bool negated = i > 0 && isNegation(tokens[i - 1]);
        for (int m = 0; m < ModuleCount; m++) {
            if (contains(words[m], tokens[i])) mentioned[m] = negated ? -1 : 1;
        }
    }
    bool anyOn = std::any_of(mentioned.begin(), mentioned.end(), [](int v) { return v == 1; });
    if (best < 0 && anyOn) {
        // No theme, but specific things asked for: build only those.
        for (int m = 0; m < ModuleCount; m++) *moduleField(plan, m) = 0;
        plan.name = "Custom Deco";
    }
    for (int m = 0; m < ModuleCount; m++) {
        int* f = moduleField(plan, m);
        if (mentioned[m] == 1) *f = std::max(*f, 2);
        if (mentioned[m] == -1) *f = 0;
    }

    // 3. Density.
    int density = 0;
    for (auto const& t : tokens) {
        if (t == "minimal" || t == "simple" || t == "clean" || t == "subtle" || t == "little" || t == "calm") density = -1;
        if (t == "dense" || t == "detailed" || t == "crazy" || t == "insane" || t == "lots" || t == "extreme" ||
            t == "busy" || t == "heavy" || t == "max" || t == "maximum" || t == "epic" || t == "full" || t == "intense") density = 1;
    }
    for (int m = 0; m < ModuleCount; m++) {
        int* f = moduleField(plan, m);
        if (*f > 0) *f = std::clamp(*f + density, 1, 3);
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
        if (t == "small" || t == "tiny" || t == "mini" || t == "little") plan.scale = 0.75f;
    }

    // Never return an empty plan.
    bool any = false;
    for (int m = 0; m < ModuleCount; m++) any = any || *moduleField(plan, m) > 0;
    if (!any) plan.particles = 1;

    for (int m = 0; m < ModuleCount; m++) {
        int* f = moduleField(plan, m);
        *f = std::clamp(*f, 0, 3);
    }
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
    matjson::Value mods = root;
    if (root.contains("modules") && root["modules"].isObject()) mods = root["modules"];

    static constexpr std::array<char const*, ModuleCount> keys = {
        "lights", "aura", "particles", "chains", "grass", "bushes", "clouds", "rods", "custom"
    };
    int read = 0;
    for (int m = 0; m < ModuleCount; m++) {
        int* f = moduleField(plan, m);
        *f = 0;
        if (mods.contains(keys[m])) {
            if (auto n = number(mods[keys[m]])) {
                *f = std::clamp(static_cast<int>(std::lround(*n)), 0, 3);
                read++;
            }
        }
    }
    if (read == 0) return false;

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

    bool any = false;
    for (int m = 0; m < ModuleCount; m++) any = any || *moduleField(plan, m) > 0;
    if (!any) plan.particles = 1;

    plan.fromAI = true;
    out = plan;
    return true;
}

std::string aiSystemPrompt() {
    return
        "You are GdGenesis, a Geometry Dash 2.2 decoration designer inside the level editor. "
        "The user selected blocks and typed a decoration prompt. Choose a decoration recipe for those blocks.\n"
        "Reply with ONLY one minified JSON object and nothing else, in this exact shape:\n"
        "{\"name\":\"short theme name\",\"main\":\"#RRGGBB\",\"accent\":\"#RRGGBB\",\"scale\":1.0,"
        "\"lights\":0,\"aura\":0,\"particles\":0,\"chains\":0,\"grass\":0,\"bushes\":0,\"clouds\":0,\"rods\":0,\"custom\":0}\n"
        "Each module is an intensity from 0 (off) to 3 (max):\n"
        "lights = small light dots along the block edges (neon, glow, outline looks)\n"
        "aura = big soft dark orbs behind the blocks for background depth\n"
        "particles = small floating dots (stars, snow, embers, bubbles, fireflies)\n"
        "chains = chains hanging under platforms\n"
        "grass = grass tufts on top edges\n"
        "bushes = bushes peeking over top edges\n"
        "clouds = clouds above the build\n"
        "rods = small spires on top edges\n"
        "custom = the user's own deco objects (only when the prompt says custom or my objects)\n"
        "scale is 0.6 to 1.6. Use 2 to 5 modules. main is the dominant deco color and accent the highlight color; "
        "match them to the prompt. Aim for tasteful, featured-quality deco.";
}

} // namespace genesis
