#include "DecoEngine.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

namespace {

// ---- Object IDs used by the generator -------------------------------------
constexpr int ID_DOT = 1764;          // small round dot (2.1 deco)
constexpr int ID_CHAIN = 41;          // long chain
constexpr int ID_CLOUD = 131;         // small cloud
constexpr int ID_ROD = 134;           // diamond rod
constexpr int ID_COLOR_TRIGGER = 899; // color trigger
constexpr int GRASS_IDS[] = { 406, 407, 408, 414 };
constexpr int BUSH_IDS[] = { 123, 124, 125, 126, 127, 128 };

// ---- Z layers (object key 24) ----------------------------------------------
constexpr int Z_B4 = -3;
constexpr int Z_B3 = -1;
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

int64_t cellKey(int x, int y) {
    return (static_cast<int64_t>(x) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(y));
}

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
            "1,{},2,{:.1f},3,{:.1f},7,{},8,{},9,{},10,0,23,{},35,1",
            ID_COLOR_TRIGGER, o.x, o.y - offsetY,
            static_cast<int>(o.rgb.r), static_cast<int>(o.rgb.g), static_cast<int>(o.rgb.b), o.target
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

} // namespace

namespace genesis {

int selectedCount() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_editorUI) return 0;
    auto sel = lel->m_editorUI->getSelectedObjects();
    return sel ? static_cast<int>(sel->count()) : 0;
}

GenerateResult generate(LevelEditorLayer* lel, DecoPlan const& plan) {
    GenerateResult res;
    if (!lel || !lel->m_editorUI) {
        res.error = "Open the level editor first.";
        return res;
    }
    auto ui = lel->m_editorUI;
    auto sel = ui->getSelectedObjects();
    if (!sel || sel->count() == 0) {
        res.error = "Select at least one block first!";
        return res;
    }

    auto mod = Mod::get();
    int mainCh = static_cast<int>(mod->getSettingValue<int64_t>("main-channel"));
    int accentCh = static_cast<int>(mod->getSettingValue<int64_t>("accent-channel"));
    bool colorTriggers = mod->getSettingValue<bool>("color-triggers");
    int group = static_cast<int>(mod->getSettingValue<int64_t>("deco-group"));
    size_t maxObjects = static_cast<size_t>(std::max<int64_t>(50, mod->getSettingValue<int64_t>("max-objects")));
    bool selectResult = mod->getSettingValue<bool>("select-result");
    bool learn = mod->getSettingValue<bool>("learn-deco");
    auto customIDs = parseIDs(mod->getSettingValue<std::string>("custom-ids"));

    Ref<CCArray> original = CCArray::createWithArray(sel);

    // ---- Read the selection -------------------------------------------------
    std::vector<CCRect> solids;
    std::vector<CCRect> others;
    std::vector<int> learned;
    for (auto obj : CCArrayExt<GameObject*>(sel)) {
        if (!obj) continue;
        CCRect r = obj->getObjectRect();
        if (r.size.width < 2.f || r.size.height < 2.f) {
            auto p = obj->getPosition();
            r = CCRect(p.x - 15.f, p.y - 15.f, 30.f, 30.f);
        }
        switch (obj->m_objectType) {
            case GameObjectType::Solid:
                solids.push_back(r);
                break;
            case GameObjectType::Hazard:
                others.push_back(r);
                break;
            case GameObjectType::Decoration:
                others.push_back(r);
                if (learn) learned.push_back(obj->m_objectID);
                break;
            default:
                // slopes and other shapes count; triggers / orbs / pads don't
                if (!typeinfo_cast<EffectGameObject*>(obj)) others.push_back(r);
                break;
        }
    }
    if (solids.empty()) solids = others;
    if (solids.empty()) {
        res.error = "Select blocks (not just triggers) to decorate.";
        return res;
    }

    // Bounding box of everything selected.
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
    std::vector<std::vector<DecoObj>> modules;

    // ---- Lights: dots along every exposed edge ----------------------------
    if (plan.lights > 0) {
        std::vector<DecoObj> v;
        int k = plan.lights;
        for (auto const& e : edges) {
            float ex = (e.cx + 0.5f) * UNIT + e.nx * UNIT * 0.5f;
            float ey = (e.cy + 0.5f) * UNIT + e.ny * UNIT * 0.5f;
            for (int i = 0; i < k; i++) {
                float t = ((i + 0.5f) / k - 0.5f) * UNIT;
                DecoObj o;
                o.id = ID_DOT;
                o.x = ex + (e.ny != 0 ? t : 0.f) + e.nx * 4.f;
                o.y = ey + (e.nx != 0 ? t : 0.f) + e.ny * 4.f;
                o.scale = (k == 3 ? 0.36f : 0.45f) * S;
                o.color = (i % 2 == 0) ? accentCh : mainCh;
                o.zLayer = Z_T1;
                o.zOrder = 20;
                if (aboveGround(o.y)) v.push_back(o);
            }
        }
        modules.push_back(std::move(v));
    }

    // ---- Aura: big soft orbs behind the build -----------------------------
    if (plan.aura > 0) {
        std::vector<DecoObj> v;
        int n = std::clamp(static_cast<int>(areaBlocks / 10.f * plan.aura), 2 * plan.aura, 80 * plan.aura);
        for (int i = 0; i < n; i++) {
            DecoObj o;
            o.id = ID_DOT;
            o.x = rf(minX - 60.f, maxX + 60.f);
            o.y = rf(std::max(minY - 30.f, groundY + 20.f), maxY + 90.f);
            o.scale = rf(2.5f, 5.5f) * S;
            o.color = mainCh;
            o.bright = rf(0.3f, 0.6f);
            o.zLayer = Z_B4;
            o.zOrder = -20 + ri(0, 5);
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Chains under exposed bottoms ---------------------------------------
    if (plan.chains > 0) {
        std::vector<DecoObj> v;
        std::unordered_set<int64_t> chosen;
        for (auto [x, y] : bottoms) {
            if (has(x, y - 1) || has(x, y - 2) || has(x, y - 3)) continue;
            float bottomY = y * UNIT;
            if (groundKnown && bottomY < groundY + 60.f) continue;
            bool near = false;
            for (int d = -2; d <= 2; d++) near = near || chosen.contains(cellKey(x + d, y));
            if (near || !chance(0.18f * plan.chains)) continue;
            chosen.insert(cellKey(x, y));
            DecoObj o;
            o.id = ID_CHAIN;
            o.x = (x + 0.5f) * UNIT;
            o.y = bottomY - 20.f;
            o.color = mainCh;
            o.bright = 0.8f;
            o.zLayer = Z_B3;
            o.zOrder = -5;
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Grass on top edges ----------------------------------------------
    if (plan.grass > 0) {
        std::vector<DecoObj> v;
        for (auto [x, y] : tops) {
            if (!chance(0.3f * plan.grass)) continue;
            DecoObj o;
            o.id = GRASS_IDS[ri(0, 3)];
            o.x = (x + 0.5f) * UNIT + rf(-7.f, 7.f);
            o.y = (y + 1) * UNIT + 7.f * S;
            o.scale = rf(0.8f, 1.15f) * S;
            o.flip = chance(0.5f);
            o.color = mainCh;
            o.zLayer = Z_T1;
            o.zOrder = 2;
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Bushes peeking over top edges --------------------------------------
    if (plan.bushes > 0) {
        std::vector<DecoObj> v;
        std::unordered_set<int64_t> chosen;
        for (auto [x, y] : tops) {
            if (has(x, y + 2)) continue;
            bool near = chosen.contains(cellKey(x - 1, y)) || chosen.contains(cellKey(x - 2, y));
            if (near || !chance(0.12f * plan.bushes)) continue;
            chosen.insert(cellKey(x, y));
            DecoObj o;
            o.id = BUSH_IDS[ri(0, 5)];
            o.x = (x + 0.5f) * UNIT;
            o.y = (y + 1) * UNIT + 12.f * S;
            o.scale = rf(0.8f, 1.2f) * S;
            o.flip = chance(0.5f);
            o.color = mainCh;
            o.bright = 0.85f;
            o.zLayer = Z_B3;
            o.zOrder = -3;
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Rods on top edges ---------------------------------------------------
    if (plan.rods > 0) {
        std::vector<DecoObj> v;
        int gap = std::max(2, 6 - plan.rods);
        std::unordered_set<int64_t> chosen;
        for (auto [x, y] : tops) {
            if (has(x, y + 2)) continue;
            bool near = false;
            for (int d = 1; d <= gap; d++) near = near || chosen.contains(cellKey(x - d, y));
            if (near || !chance(0.5f)) continue;
            chosen.insert(cellKey(x, y));
            DecoObj o;
            o.id = ID_ROD;
            o.x = (x + 0.5f) * UNIT;
            o.y = (y + 1) * UNIT + 20.f * S;
            o.scale = S;
            o.color = accentCh;
            o.zLayer = Z_B3;
            o.zOrder = -2;
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Clouds above -------------------------------------------------------
    if (plan.clouds > 0) {
        std::vector<DecoObj> v;
        int n = std::clamp(static_cast<int>(widthBlocks / 6.f * plan.clouds) + plan.clouds, plan.clouds, 40 * plan.clouds);
        for (int i = 0; i < n; i++) {
            DecoObj o;
            o.id = ID_CLOUD;
            o.x = rf(minX - 90.f, maxX + 90.f);
            o.y = rf(maxY + 60.f, maxY + 60.f + 90.f * plan.clouds);
            o.scale = rf(0.8f, 1.7f) * S;
            o.flip = chance(0.5f);
            o.color = accentCh;
            o.zLayer = Z_B4;
            o.zOrder = -30;
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Custom / learned deco -------------------------------------------
    std::vector<int> pool = customIDs;
    for (int id : learned) {
        if (std::find(pool.begin(), pool.end(), id) == pool.end()) pool.push_back(id);
    }
    if (pool.size() > 30) pool.resize(30);
    int customLevel = plan.custom;
    if (customLevel == 0 && !learned.empty()) customLevel = 1;
    if (customLevel > 0 && !pool.empty()) {
        std::vector<DecoObj> v;
        int n = std::clamp(
            static_cast<int>(edges.size() * 0.08f * customLevel + areaBlocks / 30.f * customLevel),
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
            v.push_back(o);
            i++;
        }
        modules.push_back(std::move(v));
    }

    // ---- Particles: small floating dots (last so they fill the gaps) --------
    if (plan.particles > 0) {
        std::vector<DecoObj> v;
        int n = std::clamp(static_cast<int>(widthBlocks * 0.7f * plan.particles) + 4 * plan.particles, 4, 400 * plan.particles);
        for (int tries = 0; static_cast<int>(v.size()) < n && tries < n * 4; tries++) {
            DecoObj o;
            o.id = ID_DOT;
            o.x = rf(minX - 60.f, maxX + 60.f);
            o.y = rf(minY - 15.f, maxY + 150.f);
            if (insideSolid(o.x, o.y) || !aboveGround(o.y)) continue;
            o.scale = rf(0.15f, 0.5f) * S;
            o.color = chance(0.7f) ? accentCh : mainCh;
            o.zLayer = Z_B1;
            o.zOrder = 5;
            v.push_back(o);
        }
        modules.push_back(std::move(v));
    }

    // ---- Keep under the object budget (shrink every module evenly) ---------
    size_t budget = maxObjects > 2 ? maxObjects - 2 : maxObjects;
    size_t total = 0;
    for (auto const& m : modules) total += m.size();
    if (total > budget) {
        double ratio = static_cast<double>(budget) / static_cast<double>(total);
        for (auto& m : modules) {
            std::shuffle(m.begin(), m.end(), rng());
            size_t keep = std::max<size_t>(1, static_cast<size_t>(m.size() * ratio));
            if (m.size() > keep) m.resize(keep);
        }
    }

    std::vector<DecoObj> all;
    if (colorTriggers) {
        DecoObj a;
        a.trigger = true;
        a.x = std::max(15.f, minX - 45.f);
        a.y = maxY + 45.f;
        a.rgb = plan.main;
        a.target = mainCh;
        all.push_back(a);
        DecoObj b = a;
        b.y = maxY + 75.f;
        b.rgb = plan.accent;
        b.target = accentCh;
        all.push_back(b);
    }
    for (auto const& m : modules) all.insert(all.end(), m.begin(), m.end());
    if (all.empty() || (colorTriggers && all.size() <= 2)) {
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

    ui->deselectAll();
    ui->selectObjects(selectResult ? created : original.data(), true);
    ui->updateButtons();

    res.ok = true;
    res.created = static_cast<int>(created->count());
    return res;
}

} // namespace genesis
