#include "DecoEngine.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

namespace {

// ---- Object IDs used by the generator -------------------------------------
constexpr int ID_TINY_CIRCLE = 1764;
constexpr int ID_GLOW_ORB_S = 1887;
constexpr int ID_GLOW_ORB_M = 1886;
constexpr int ID_GLOW_ORB_L = 1888;
constexpr int ID_SPARKLE = 1519;
constexpr int ID_GLOW_SMALL = 1292;  // straight small glow
constexpr int ID_GLOW_MEDIUM = 503;  // straight medium glow
constexpr int ID_GLOW_LARGE = 1011;  // straight large glow
constexpr int ID_OUTLINE_TILE = 467; // outline square
constexpr int ID_COLOR_SQUARE = 211; // plain colored square
constexpr int ID_COLOR_TRIGGER = 899;
constexpr int CHAIN_IDS[] = { 41, 41, 110, 107 };
constexpr int CLOUD_IDS[] = { 129, 130, 131, 222, 223, 224 };
constexpr int ROD_IDS[] = { 134, 190, 16, 17 };
constexpr int GRASS_IDS[] = { 406, 407, 408, 414, 940, 941, 942 };
constexpr int BUSH_IDS[] = { 907, 908, 909, 910 };

// ---- Color channels for the level ---------------------------------------
constexpr int CH_BG = 1000;
constexpr int CH_GROUND = 1001;
constexpr int CH_LINE = 1002;
constexpr int CH_GROUND2 = 1009;

// ---- Z layers (object key 24) ----------------------------------------------
constexpr int Z_B4 = -3;
constexpr int Z_B3 = -1;
constexpr int Z_B2 = 1;
constexpr int Z_B1 = 3;
constexpr int Z_T1 = 5;

constexpr float UNIT = 30.f;

struct DecoObj {
    int id = 0;
    float x = 0.f, y = 0.f;
    float rot = 0.f;
    float scale = 1.f;
    int color = 0;
    int zLayer = Z_B1;
    int zOrder = 0;
    bool flip = false;
    float bright = -1.f; // HSV brightness multiplier, -1 = untouched

    // color triggers only
    bool trigger = false;
    ccColor3B rgb {};
    int target = 0;
    float opacity = 1.f;
    float duration = 0.f;
};

struct ModuleOut {
    std::vector<DecoObj> objs;
    bool ordered = false; // trim evenly instead of randomly
};

struct Edge {
    int cx, cy; // cell
    int nx, ny; // outward normal
};

std::mt19937& rng() {
    static std::mt19937 r { std::random_device {}() };
    return r;
}
float rf(float a, float b) {
    if (b <= a) return a;
    return std::uniform_real_distribution<float>(a, b)(rng());
}
int ri(int a, int b) {
    if (b <= a) return a;
    return std::uniform_int_distribution<int>(a, b)(rng());
}
bool chance(float p) {
    return rf(0.f, 1.f) < p;
}
template <class T, size_t N>
T pick(T const (&arr)[N]) {
    return arr[ri(0, static_cast<int>(N) - 1)];
}

int64_t cellKey(int x, int y) {
    return (static_cast<int64_t>(x) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(y));
}

float rotationFor(int nx, int ny) {
    if (ny > 0) return 0.f;
    if (nx > 0) return 90.f;
    if (ny < 0) return 180.f;
    return 270.f;
}

// ---- Colors ---------------------------------------------------------------
struct HSV { float h, s, v; };

HSV toHSV(ccColor3B c) {
    float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    float mx = std::max({ r, g, b }), mn = std::min({ r, g, b });
    float d = mx - mn;
    float h = 0.f;
    if (d > 1e-5f) {
        if (mx == r) h = 60.f * std::fmod((g - b) / d, 6.f);
        else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
        else h = 60.f * ((r - g) / d + 4.f);
    }
    if (h < 0.f) h += 360.f;
    return { h, mx <= 0.f ? 0.f : d / mx, mx };
}

ccColor3B fromHSV(HSV c) {
    float h = std::fmod(c.h, 360.f);
    if (h < 0.f) h += 360.f;
    float s = std::clamp(c.s, 0.f, 1.f), v = std::clamp(c.v, 0.f, 1.f);
    float C = v * s;
    float X = C * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    float m = v - C;
    float r = 0, g = 0, b = 0;
    if (h < 60) { r = C; g = X; }
    else if (h < 120) { r = X; g = C; }
    else if (h < 180) { g = C; b = X; }
    else if (h < 240) { g = X; b = C; }
    else if (h < 300) { r = X; b = C; }
    else { r = C; b = X; }
    auto to = [m](float f) { return static_cast<GLubyte>(std::clamp(std::lround((f + m) * 255.f), 0L, 255L)); };
    return { to(r), to(g), to(b) };
}

ccColor3B hueShift(ccColor3B c, float degrees) {
    auto hsv = toHSV(c);
    hsv.h += degrees;
    return fromHSV(hsv);
}

ccColor3B darken(ccColor3B c, float v) {
    auto hsv = toHSV(c);
    hsv.v = std::min(hsv.v, v);
    hsv.s = std::min(1.f, hsv.s * 0.95f + 0.05f);
    return fromHSV(hsv);
}

// ---- Helpers ----------------------------------------------------------------
std::vector<int> parseIDs(std::string const& text) {
    std::vector<int> ids;
    int cur = 0;
    bool has = false;
    for (char c : text + ",") {
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            has = true;
            if (cur > 100000) cur = 100000;
        }
        else {
            if (has && cur > 0 && cur < 10000) ids.push_back(cur);
            cur = 0;
            has = false;
        }
    }
    return ids;
}

std::string objectString(DecoObj const& o, float offsetY, int group) {
    if (o.trigger) {
        return fmt::format(
            "1,{},2,{:.1f},3,{:.1f},7,{},8,{},9,{},10,{:.2f},23,{},35,{:.2f}",
            ID_COLOR_TRIGGER, o.x, o.y - offsetY,
            static_cast<int>(o.rgb.r), static_cast<int>(o.rgb.g), static_cast<int>(o.rgb.b),
            o.duration, o.target, o.opacity
        );
    }
    std::string s = fmt::format("1,{},2,{:.2f},3,{:.2f}", o.id, o.x, o.y - offsetY);
    if (std::abs(o.rot) > 0.01f) s += fmt::format(",6,{:.1f}", o.rot);
    if (std::abs(o.scale - 1.f) > 0.01f) s += fmt::format(",32,{0:.2f},128,{0:.2f},129,{0:.2f}", o.scale);
    if (o.flip) s += ",4,1";
    if (o.color > 0) s += fmt::format(",21,{}", o.color);
    s += fmt::format(",24,{},25,{}", o.zLayer, o.zOrder);
    if (group > 0) s += fmt::format(",57,{}", group);
    if (o.bright >= 0.f) s += fmt::format(",41,1,43,0a1a{:.2f}a0a0", o.bright);
    return s;
}

void trimEven(std::vector<DecoObj>& v, size_t keep) {
    if (v.size() <= keep) return;
    std::vector<DecoObj> out;
    out.reserve(keep);
    double step = static_cast<double>(v.size()) / static_cast<double>(keep);
    for (size_t i = 0; i < keep; i++) out.push_back(v[static_cast<size_t>(i * step)]);
    v.swap(out);
}

bool isTarget(GameObject* obj) {
    if (!obj) return false;
    switch (obj->m_objectType) {
        case GameObjectType::Solid:
        case GameObjectType::Hazard:
            return true;
        case GameObjectType::Decoration:
            return false;
        default:
            return !typeinfo_cast<EffectGameObject*>(obj);
    }
}

} // namespace

namespace genesis {

int selectedCount() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_editorUI) return 0;
    auto sel = lel->m_editorUI->getSelectedObjects();
    return sel ? static_cast<int>(sel->count()) : 0;
}

int levelBlockCount() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_objects) return 0;
    int n = 0;
    for (auto obj : CCArrayExt<GameObject*>(lel->m_objects)) {
        if (obj && obj->m_objectType == GameObjectType::Solid) n++;
    }
    return n;
}

GenerateResult generate(LevelEditorLayer* lel, DecoPlan const& plan, bool wholeLevel) {
    GenerateResult res;
    if (!lel || !lel->m_editorUI) {
        res.error = "Open the level editor first.";
        return res;
    }
    auto ui = lel->m_editorUI;

    auto mod = Mod::get();
    int mainCh = static_cast<int>(mod->getSettingValue<int64_t>("main-channel"));
    int accentCh = static_cast<int>(mod->getSettingValue<int64_t>("accent-channel"));
    int fillCh = static_cast<int>(mod->getSettingValue<int64_t>("fill-channel"));
    bool colorTriggers = mod->getSettingValue<bool>("color-triggers");
    int group = static_cast<int>(mod->getSettingValue<int64_t>("deco-group"));
    size_t maxObjects = static_cast<size_t>(std::max<int64_t>(50, mod->getSettingValue<int64_t>("max-objects")));
    bool selectResult = mod->getSettingValue<bool>("select-result");
    bool learn = mod->getSettingValue<bool>("learn-deco") && !wholeLevel;
    auto customIDs = parseIDs(mod->getSettingValue<std::string>("custom-ids"));

    // ---- What are we decorating? -------------------------------------------
    // Shallow copy (CCArray::createWithArray deep-copies and crashes on GameObjects).
    Ref<CCArray> original = CCArray::create();
    if (auto sel = ui->getSelectedObjects()) original->addObjectsFromArray(sel);

    CCArray* source = wholeLevel ? lel->m_objects : original.data();
    if (!source || source->count() == 0) {
        res.error = wholeLevel ? "This level has no blocks yet - build a layout first!" : "Select at least one block first!";
        return res;
    }

    std::vector<CCRect> solids;
    std::vector<CCRect> others;
    std::vector<int> learned;
    std::unordered_map<int, int> solidIDs;
    for (auto obj : CCArrayExt<GameObject*>(source)) {
        if (!obj) continue;
        if (!isTarget(obj)) {
            if (learn && obj->m_objectType == GameObjectType::Decoration) learned.push_back(obj->m_objectID);
            continue;
        }
        CCRect r = obj->getObjectRect();
        if (r.size.width < 2.f || r.size.height < 2.f) {
            auto p = obj->getPosition();
            r = CCRect(p.x - 15.f, p.y - 15.f, 30.f, 30.f);
        }
        if (obj->m_objectType == GameObjectType::Solid) {
            solids.push_back(r);
            solidIDs[obj->m_objectID]++;
        }
        else {
            others.push_back(r);
        }
    }
    if (solids.empty()) solids = others;
    if (solids.empty()) {
        res.error = wholeLevel ? "No blocks found in this level." : "Select blocks (not just triggers) to decorate.";
        return res;
    }

    int fillBlockID = 1;
    int bestCount = 0;
    for (auto const& [id, count] : solidIDs) {
        if (count > bestCount) { bestCount = count; fillBlockID = id; }
    }

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    auto grow = [&](CCRect const& r) {
        minX = std::min(minX, r.getMinX());
        minY = std::min(minY, r.getMinY());
        maxX = std::max(maxX, r.getMaxX());
        maxY = std::max(maxY, r.getMaxY());
    };
    for (auto const& r : solids) grow(r);
    for (auto const& r : others) grow(r);
    float bw = maxX - minX;
    float bh = maxY - minY;
    float widthBlocks = std::max(1.f, bw / UNIT);
    float areaBlocks = std::max(1.f, (bw + 120.f) * (bh + 120.f) / (UNIT * UNIT));

    // In the editor the ground top sits at y = 90; keep deco above it.
    bool groundKnown = minY >= 89.f;
    float groundY = groundKnown ? 92.f : -1e9f;

    // ---- Grid of solid cells + exposed edges ---------------------------------
    std::unordered_set<int64_t> cells;
    std::vector<std::pair<int, int>> cellList;
    for (auto const& r : solids) {
        int x0 = static_cast<int>(std::floor((r.getMinX() + 1.f) / UNIT));
        int x1 = static_cast<int>(std::floor((r.getMaxX() - 1.f) / UNIT));
        int y0 = static_cast<int>(std::floor((r.getMinY() + 1.f) / UNIT));
        int y1 = static_cast<int>(std::floor((r.getMaxY() - 1.f) / UNIT));
        if (x1 < x0 || y1 < y0) continue;
        if ((x1 - x0 + 1) * (y1 - y0 + 1) > 2500) continue; // huge scaled object: skip
        for (int x = x0; x <= x1; x++) {
            for (int y = y0; y <= y1; y++) {
                if (cells.insert(cellKey(x, y)).second) cellList.push_back({ x, y });
            }
        }
    }
    std::sort(cellList.begin(), cellList.end());
    auto has = [&](int x, int y) { return cells.contains(cellKey(x, y)); };

    std::vector<Edge> edges;
    std::vector<std::pair<int, int>> tops, bottoms;
    for (auto [x, y] : cellList) {
        if (!has(x, y + 1)) { edges.push_back({ x, y, 0, 1 }); tops.push_back({ x, y }); }
        if (!has(x, y - 1)) { edges.push_back({ x, y, 0, -1 }); bottoms.push_back({ x, y }); }
        if (!has(x - 1, y)) edges.push_back({ x, y, -1, 0 });
        if (!has(x + 1, y)) edges.push_back({ x, y, 1, 0 });
    }

    auto insideSolid = [&](float x, float y) {
        return has(static_cast<int>(std::floor(x / UNIT)), static_cast<int>(std::floor(y / UNIT)));
    };
    auto aboveGround = [&](float y) { return y >= groundY; };

    float S = std::clamp(plan.scale, 0.5f, 1.8f);
    float D = std::clamp(plan.density, 0.25f, 4.f);
    bool epicPlus = plan.tier >= Tier::Epic;
    std::vector<ModuleOut> modules;
    bool fillUsesChannel = false;
    float fillOpacity = 1.f;
    ccColor3B fillColor = plan.main;

    auto glowPieceFor = [](int level) {
        return level >= 3 ? ID_GLOW_LARGE : (level == 2 ? ID_GLOW_MEDIUM : ID_GLOW_SMALL);
    };

    // ---- Edge glow: glow pieces along the outside of every edge ------------
    if (plan[M_Glow] > 0) {
        ModuleOut m;
        m.ordered = true;
        int id = glowPieceFor(plan[M_Glow]);
        for (auto const& e : edges) {
            DecoObj o;
            o.id = id;
            o.x = (e.cx + e.nx + 0.5f) * UNIT;
            o.y = (e.cy + e.ny + 0.5f) * UNIT;
            o.rot = rotationFor(e.nx, e.ny);
            o.color = mainCh;
            o.zLayer = Z_B2;
            o.zOrder = -10;
            if (aboveGround(o.y - 14.f)) m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Lights: dots along every exposed edge (+ soft halos) -------------
    if (plan[M_Lights] > 0) {
        ModuleOut m, halos;
        m.ordered = true;
        halos.ordered = true;
        int k = plan[M_Lights];
        for (auto const& e : edges) {
            float ex = (e.cx + 0.5f) * UNIT + e.nx * UNIT * 0.5f;
            float ey = (e.cy + 0.5f) * UNIT + e.ny * UNIT * 0.5f;
            for (int i = 0; i < k; i++) {
                float t = ((i + 0.5f) / k - 0.5f) * UNIT;
                DecoObj o;
                o.id = ID_TINY_CIRCLE;
                o.x = ex + (e.ny != 0 ? t : 0.f) + e.nx * 4.f;
                o.y = ey + (e.nx != 0 ? t : 0.f) + e.ny * 4.f;
                o.scale = (k == 3 ? 0.36f : 0.45f) * S;
                o.color = (i % 2 == 0) ? accentCh : mainCh;
                o.zLayer = Z_T1;
                o.zOrder = 20;
                if (!aboveGround(o.y)) continue;
                m.objs.push_back(o);
                if (plan.halos && i == 0 && ((e.cx + e.cy) % 2 == 0)) {
                    DecoObj h = o;
                    h.id = ID_GLOW_ORB_S;
                    h.scale = 0.55f * S;
                    h.color = accentCh;
                    h.zLayer = Z_B1;
                    h.zOrder = 1;
                    halos.objs.push_back(h);
                }
            }
        }
        modules.push_back(std::move(m));
        if (!halos.objs.empty()) modules.push_back(std::move(halos));
    }

    // ---- Fill: decoration inside the blocks ---------------------------------
    if (plan[M_Fill] > 0) {
        ModuleOut m;
        m.ordered = true;
        int f = plan[M_Fill];
        switch (plan.fillStyle) {
            case FillStyle::Glow: {
                fillUsesChannel = true;
                fillColor = plan.main;
                fillOpacity = 1.f;
                int id = glowPieceFor(f);
                for (auto const& e : edges) {
                    DecoObj o;
                    o.id = id;
                    o.x = (e.cx + 0.5f) * UNIT;
                    o.y = (e.cy + 0.5f) * UNIT;
                    o.rot = rotationFor(e.nx, e.ny) + 180.f;
                    if (o.rot >= 360.f) o.rot -= 360.f;
                    o.color = fillCh;
                    o.zLayer = Z_T1;
                    o.zOrder = 1;
                    m.objs.push_back(o);
                }
                break;
            }
            case FillStyle::Rim: {
                int n = 1 + f;
                for (auto const& e : edges) {
                    float ex = (e.cx + 0.5f) * UNIT + e.nx * (UNIT * 0.5f - 6.f);
                    float ey = (e.cy + 0.5f) * UNIT + e.ny * (UNIT * 0.5f - 6.f);
                    for (int i = 0; i < n; i++) {
                        float t = ((i + 0.5f) / n - 0.5f) * (UNIT - 8.f);
                        DecoObj o;
                        o.id = ID_TINY_CIRCLE;
                        o.x = ex + (e.ny != 0 ? t : 0.f);
                        o.y = ey + (e.nx != 0 ? t : 0.f);
                        o.scale = 0.28f * S;
                        o.color = accentCh;
                        o.zLayer = Z_T1;
                        o.zOrder = 12;
                        m.objs.push_back(o);
                    }
                }
                break;
            }
            case FillStyle::Dots: {
                fillUsesChannel = true;
                fillColor = plan.accent;
                fillOpacity = 0.85f;
                int g = f >= 3 ? 3 : 2;
                for (auto [x, y] : cellList) {
                    for (int i = 0; i < g; i++) {
                        for (int j = 0; j < g; j++) {
                            DecoObj o;
                            o.id = ID_TINY_CIRCLE;
                            o.x = (x + (i + 0.5f) / g) * UNIT;
                            o.y = (y + (j + 0.5f) / g) * UNIT;
                            o.scale = (g == 3 ? 0.22f : 0.3f) * S;
                            o.color = (plan.tier == Tier::Mythic && ((i + j) % 2 == 1)) ? mainCh : fillCh;
                            o.zLayer = Z_T1;
                            o.zOrder = 11;
                            m.objs.push_back(o);
                        }
                    }
                }
                break;
            }
            case FillStyle::Tiles: {
                fillUsesChannel = true;
                fillColor = plan.main;
                fillOpacity = 0.7f;
                float sc = f == 1 ? 0.9f : (f == 2 ? 0.82f : 0.72f);
                for (auto [x, y] : cellList) {
                    DecoObj o;
                    o.id = ID_OUTLINE_TILE;
                    o.x = (x + 0.5f) * UNIT;
                    o.y = (y + 0.5f) * UNIT;
                    o.scale = sc;
                    o.color = fillCh;
                    o.zLayer = Z_T1;
                    o.zOrder = 10;
                    m.objs.push_back(o);
                }
                break;
            }
            case FillStyle::Checker: {
                fillUsesChannel = true;
                fillColor = plan.main;
                fillOpacity = f >= 3 ? 0.5f : 0.35f;
                for (auto [x, y] : cellList) {
                    if (((x + y) % 2 + 2) % 2 != 0) continue;
                    DecoObj o;
                    o.id = ID_COLOR_SQUARE;
                    o.x = (x + 0.5f) * UNIT;
                    o.y = (y + 0.5f) * UNIT;
                    o.color = fillCh;
                    o.zLayer = Z_T1;
                    o.zOrder = 9;
                    m.objs.push_back(o);
                }
                break;
            }
            case FillStyle::Solid: {
                // Fill empty cells that are fully enclosed by blocks.
                if (!cellList.empty()) {
                    int cx0 = cellList.front().first, cx1 = cellList.back().first;
                    int cy0 = cellList.front().second, cy1 = cellList.front().second;
                    for (auto [x, y] : cellList) { cy0 = std::min(cy0, y); cy1 = std::max(cy1, y); }
                    int64_t area = static_cast<int64_t>(cx1 - cx0 + 1) * (cy1 - cy0 + 1);
                    if (area <= 400000) {
                        std::unordered_set<int64_t> outside;
                        std::deque<std::pair<int, int>> q;
                        auto seed = [&](int x, int y) {
                            if (x < cx0 || x > cx1 || y < cy0 || y > cy1) return;
                            if (has(x, y) || outside.contains(cellKey(x, y))) return;
                            outside.insert(cellKey(x, y));
                            q.push_back({ x, y });
                        };
                        for (int x = cx0; x <= cx1; x++) { seed(x, cy0); seed(x, cy1); }
                        for (int y = cy0; y <= cy1; y++) { seed(cx0, y); seed(cx1, y); }
                        while (!q.empty()) {
                            auto [x, y] = q.front();
                            q.pop_front();
                            seed(x + 1, y); seed(x - 1, y); seed(x, y + 1); seed(x, y - 1);
                        }
                        for (int x = cx0; x <= cx1; x++) {
                            for (int y = cy0; y <= cy1; y++) {
                                if (has(x, y) || outside.contains(cellKey(x, y))) continue;
                                DecoObj o;
                                o.id = fillBlockID;
                                o.x = (x + 0.5f) * UNIT;
                                o.y = (y + 0.5f) * UNIT;
                                o.zLayer = Z_B1;
                                o.zOrder = 0;
                                m.objs.push_back(o);
                            }
                        }
                    }
                }
                break;
            }
        }
        modules.push_back(std::move(m));
    }

    // ---- Aura: big soft glow orbs behind the build -------------------------
    if (plan[M_Aura] > 0) {
        ModuleOut m;
        int a = plan[M_Aura];
        int n = std::clamp(static_cast<int>(areaBlocks / 12.f * a * D), 2 * a, 150 * a);
        for (int i = 0; i < n; i++) {
            DecoObj o;
            o.id = chance(0.7f) ? ID_GLOW_ORB_L : ID_GLOW_ORB_M;
            o.x = rf(minX - 60.f, maxX + 60.f);
            o.y = rf(std::max(minY - 30.f, groundY + 20.f), maxY + 90.f);
            o.scale = rf(1.f, 2.4f) * S;
            o.color = chance(0.75f) ? mainCh : accentCh;
            o.bright = rf(0.45f, 0.8f);
            o.zLayer = Z_B4;
            o.zOrder = -20 + ri(0, 5);
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Chains under exposed bottoms ---------------------------------------
    if (plan[M_Chains] > 0) {
        ModuleOut m;
        std::unordered_set<int64_t> chosen;
        float p = std::min(0.9f, 0.18f * plan[M_Chains] * D);
        for (auto [x, y] : bottoms) {
            if (has(x, y - 1) || has(x, y - 2) || has(x, y - 3)) continue;
            float bottomY = y * UNIT;
            if (groundKnown && bottomY < groundY + 60.f) continue;
            bool blocked = false;
            for (int d = -2; d <= 2; d++) blocked = blocked || chosen.contains(cellKey(x + d, y));
            if (blocked || !chance(p)) continue;
            chosen.insert(cellKey(x, y));
            DecoObj o;
            o.id = pick(CHAIN_IDS);
            o.x = (x + 0.5f) * UNIT;
            o.y = bottomY - 20.f;
            o.color = mainCh;
            o.bright = 0.8f;
            o.zLayer = Z_B3;
            o.zOrder = -5;
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Grass on top edges ----------------------------------------------
    if (plan[M_Grass] > 0) {
        ModuleOut m;
        float p = std::min(0.95f, 0.3f * plan[M_Grass] * D);
        for (auto [x, y] : tops) {
            if (!chance(p)) continue;
            DecoObj o;
            o.id = pick(GRASS_IDS);
            o.x = (x + 0.5f) * UNIT + rf(-7.f, 7.f);
            o.y = (y + 1) * UNIT + 7.f * S;
            o.scale = rf(0.8f, 1.15f) * S;
            o.flip = chance(0.5f);
            o.color = mainCh;
            o.zLayer = Z_T1;
            o.zOrder = 2;
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Bushes peeking over top edges --------------------------------------
    if (plan[M_Bushes] > 0) {
        ModuleOut m;
        std::unordered_set<int64_t> chosen;
        float p = std::min(0.8f, 0.12f * plan[M_Bushes] * D);
        for (auto [x, y] : tops) {
            if (has(x, y + 2)) continue;
            bool blocked = chosen.contains(cellKey(x - 1, y)) || chosen.contains(cellKey(x - 2, y));
            if (blocked || !chance(p)) continue;
            chosen.insert(cellKey(x, y));
            DecoObj o;
            o.id = pick(BUSH_IDS);
            o.x = (x + 0.5f) * UNIT;
            o.y = (y + 1) * UNIT + 12.f * S;
            o.scale = rf(0.8f, 1.2f) * S;
            o.flip = chance(0.5f);
            o.color = mainCh;
            o.bright = 0.85f;
            o.zLayer = Z_B3;
            o.zOrder = -3;
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Rods on top edges ---------------------------------------------------
    if (plan[M_Rods] > 0) {
        ModuleOut m;
        int gap = std::max(2, 6 - plan[M_Rods]);
        std::unordered_set<int64_t> chosen;
        float p = std::min(0.9f, 0.5f * D);
        for (auto [x, y] : tops) {
            if (has(x, y + 2)) continue;
            bool blocked = false;
            for (int d = 1; d <= gap; d++) blocked = blocked || chosen.contains(cellKey(x - d, y));
            if (blocked || !chance(p)) continue;
            chosen.insert(cellKey(x, y));
            DecoObj o;
            o.id = pick(ROD_IDS);
            o.x = (x + 0.5f) * UNIT;
            o.y = (y + 1) * UNIT + 20.f * S;
            o.scale = S;
            o.color = accentCh;
            o.zLayer = Z_B3;
            o.zOrder = -2;
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Clouds above -------------------------------------------------------
    if (plan[M_Clouds] > 0) {
        ModuleOut m;
        int c = plan[M_Clouds];
        int n = std::clamp(static_cast<int>(widthBlocks / 6.f * c * D) + c, c, 60 * c);
        for (int i = 0; i < n; i++) {
            DecoObj o;
            o.id = pick(CLOUD_IDS);
            o.x = rf(minX - 90.f, maxX + 90.f);
            o.y = rf(maxY + 60.f, maxY + 60.f + 90.f * c);
            o.scale = rf(0.8f, 1.6f) * S;
            o.flip = chance(0.5f);
            o.color = accentCh;
            o.zLayer = Z_B4;
            o.zOrder = -30;
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Custom / learned deco -------------------------------------------
    std::vector<int> pool = customIDs;
    for (int id : learned) {
        if (std::find(pool.begin(), pool.end(), id) == pool.end()) pool.push_back(id);
    }
    if (pool.size() > 30) pool.resize(30);
    int customLevel = plan[M_Custom];
    if (customLevel == 0 && !learned.empty()) customLevel = 1;
    if (customLevel > 0 && !pool.empty()) {
        ModuleOut m;
        int n = std::clamp(
            static_cast<int>((edges.size() * 0.08f * customLevel + areaBlocks / 30.f * customLevel) * D),
            2 * customLevel, 300 * customLevel
        );
        for (int i = 0, tries = 0; i < n && tries < n * 4; tries++) {
            DecoObj o;
            o.id = pool[ri(0, static_cast<int>(pool.size()) - 1)];
            if (!edges.empty() && (i % 2 == 0)) {
                auto const& e = edges[ri(0, static_cast<int>(edges.size()) - 1)];
                o.x = (e.cx + 0.5f) * UNIT + e.nx * (UNIT * 0.5f + 8.f) + (e.ny != 0 ? rf(-12.f, 12.f) : 0.f);
                o.y = (e.cy + 0.5f) * UNIT + e.ny * (UNIT * 0.5f + 8.f) + (e.nx != 0 ? rf(-12.f, 12.f) : 0.f);
            }
            else {
                o.x = rf(minX - 45.f, maxX + 45.f);
                o.y = rf(minY - 15.f, maxY + 90.f);
                if (insideSolid(o.x, o.y)) continue;
            }
            if (!aboveGround(o.y)) continue;
            o.rot = 90.f * ri(0, 3);
            o.scale = rf(0.6f, 1.15f) * S;
            o.color = chance(0.5f) ? mainCh : accentCh;
            o.zLayer = chance(0.5f) ? Z_B1 : Z_T1;
            o.zOrder = ri(-5, 5);
            m.objs.push_back(o);
            i++;
        }
        modules.push_back(std::move(m));
    }

    // ---- Particles: floating dots, glow specks and sparkles ----------------
    if (plan[M_Particles] > 0) {
        ModuleOut m;
        int p = plan[M_Particles];
        int n = std::clamp(static_cast<int>((widthBlocks * 0.7f * p + 4 * p) * D), 4, 500 * p);
        for (int tries = 0; static_cast<int>(m.objs.size()) < n && tries < n * 4; tries++) {
            DecoObj o;
            o.x = rf(minX - 60.f, maxX + 60.f);
            o.y = rf(minY - 15.f, maxY + 150.f);
            if (insideSolid(o.x, o.y) || !aboveGround(o.y)) continue;
            float roll = rf(0.f, 1.f);
            if (plan.sparkles && roll < 0.1f) {
                o.id = ID_SPARKLE;
                o.scale = rf(0.5f, 0.9f) * S;
            }
            else if (epicPlus && roll < 0.45f) {
                o.id = ID_GLOW_ORB_S;
                o.scale = rf(0.2f, 0.5f) * S;
            }
            else {
                o.id = ID_TINY_CIRCLE;
                o.scale = rf(0.15f, 0.45f) * S;
            }
            o.color = chance(0.7f) ? accentCh : mainCh;
            o.zLayer = Z_B1;
            o.zOrder = 5;
            m.objs.push_back(o);
        }
        modules.push_back(std::move(m));
    }

    // ---- Color triggers: palette per section (+ background & ground) -------
    std::vector<DecoObj> triggers;
    if (colorTriggers) {
        int sections = std::max(1, plan.sections);
        static constexpr float kShift[] = { 0.f, 35.f, -30.f, 70.f, -60.f, 110.f, -95.f, 150.f };
        float span = std::max(UNIT, bw) / sections;
        for (int s = 0; s < sections; s++) {
            float shift = kShift[s % std::size(kShift)];
            ccColor3B mainC = hueShift(plan.main, shift);
            ccColor3B accentC = hueShift(plan.accent, shift);
            ccColor3B fillC = hueShift(fillColor, shift);
            float x = s == 0 ? std::max(15.f, minX - 45.f) : minX + span * s;
            float dur = s == 0 ? 0.f : 0.8f;
            int k = 0;
            auto add = [&](int target, ccColor3B c, float opacity) {
                DecoObj t;
                t.trigger = true;
                t.x = x;
                t.y = maxY + 45.f + 30.f * k++;
                t.rgb = c;
                t.target = target;
                t.opacity = opacity;
                t.duration = dur;
                triggers.push_back(t);
            };
            add(mainCh, mainC, 1.f);
            add(accentCh, accentC, 1.f);
            if (fillUsesChannel && plan[M_Fill] > 0) add(fillCh, fillC, fillOpacity);
            if (plan[M_Background] > 0) {
                add(CH_BG, darken(mainC, 0.16f), 1.f);
                add(CH_GROUND, darken(mainC, 0.3f), 1.f);
                add(CH_GROUND2, darken(accentC, 0.24f), 1.f);
                add(CH_LINE, accentC, 1.f);
            }
        }
    }

    // ---- Keep under the object budget (shrink every module evenly) ---------
    size_t budget = maxObjects > triggers.size() + 1 ? maxObjects - triggers.size() : 1;
    size_t total = 0;
    for (auto const& m : modules) total += m.objs.size();
    if (total > budget) {
        double ratio = static_cast<double>(budget) / static_cast<double>(total);
        for (auto& m : modules) {
            size_t keep = std::max<size_t>(1, static_cast<size_t>(m.objs.size() * ratio));
            if (m.objs.size() <= keep) continue;
            if (m.ordered) {
                trimEven(m.objs, keep);
            }
            else {
                std::shuffle(m.objs.begin(), m.objs.end(), rng());
                m.objs.resize(keep);
            }
        }
    }

    std::vector<DecoObj> all = triggers;
    for (auto const& m : modules) all.insert(all.end(), m.objs.begin(), m.objs.end());
    if (all.size() <= triggers.size()) {
        res.error = "Nothing to place here - try a different prompt.";
        return res;
    }

    // ---- Build the object string and add it to the level ------------------
    // Level-string Y is 90 lower than editor Y; corrected below if needed.
    constexpr float STRING_OFFSET_Y = 90.f;
    std::string str;
    str.reserve(all.size() * 64);
    for (size_t i = 0; i < all.size(); i++) {
        if (i) str += ';';
        str += objectString(all[i], STRING_OFFSET_Y, all[i].trigger ? 0 : group);
    }

    auto created = lel->createObjectsFromString(gd::string(str.c_str()), false, false);
    if (!created || created->count() == 0) {
        res.error = "The editor didn't accept the objects (object limit reached?).";
        return res;
    }

    // Make sure everything landed exactly where it was planned.
    auto first = static_cast<GameObject*>(created->objectAtIndex(0));
    if (first && first->m_objectID == (all[0].trigger ? ID_COLOR_TRIGGER : all[0].id)) {
        CCPoint delta = CCPoint(all[0].x, all[0].y) - first->getPosition();
        if (std::abs(delta.x) > 0.5f || std::abs(delta.y) > 0.5f) {
            for (auto obj : CCArrayExt<GameObject*>(created)) {
                ui->moveObject(obj, delta);
            }
        }
    }

    // Fill pieces are visual only: never let them block the player.
    for (auto obj : CCArrayExt<GameObject*>(created)) {
        if (obj && (obj->m_objectType == GameObjectType::Solid || obj->m_objectType == GameObjectType::Hazard)) {
            obj->m_isNoTouch = true;
        }
    }

    ui->deselectAll();
    if (selectResult) {
        ui->selectObjects(created, true);
    }
    else if (!wholeLevel && original->count() > 0) {
        ui->selectObjects(original.data(), true);
    }
    ui->updateButtons();

    res.ok = true;
    res.created = static_cast<int>(created->count());
    return res;
}

} // namespace genesis
