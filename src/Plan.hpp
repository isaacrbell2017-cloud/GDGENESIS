#pragma once

#include <Geode/Geode.hpp>
#include <array>
#include <string>

// How the inside of blocks gets filled.
enum class FillStyle : int {
    Glow = 0,    // soft glow along the inside edges
    Rim = 1,     // light dots just inside the edges
    Dots = 2,    // LED dot grid
    Tiles = 3,   // outline tile on every block
    Checker = 4, // tinted checkerboard
    Solid = 5,   // fill enclosed holes with your block
};
constexpr int kFillStyleCount = 6;
char const* fillStyleName(FillStyle s);

// Quality tiers, like GD's level ratings.
enum class Tier : int { Featured = 0, Epic = 1, Legendary = 2, Mythic = 3 };
char const* tierName(Tier t);

// The modules the generator can build. Order matters (saved settings use it).
enum Module : int {
    M_Lights, M_Glow, M_Fill, M_Aura, M_Particles, M_Chains,
    M_Grass, M_Bushes, M_Clouds, M_Rods, M_Background, M_Custom,
    M_Count
};
char const* moduleName(int m);
constexpr int kUserModuleCount = M_Background + 1; // modules shown on the Modules page

// A decoration "recipe". Every module is an intensity from 0 (off) to 3 (max).
struct DecoPlan {
    std::string name = "Modern Glow";
    cocos2d::ccColor3B main { 0, 200, 255 };
    cocos2d::ccColor3B accent { 255, 255, 255 };

    std::array<int, M_Count> mods {};
    FillStyle fillStyle = FillStyle::Glow;

    float scale = 1.f;
    float density = 1.f;
    int sections = 1;     // color sections along the level (whole-level mode)
    bool halos = false;   // soft glow behind light dots
    bool sparkles = false;
    Tier tier = Tier::Featured;
    int promptTier = -1;          // tier named in the prompt ("mythic"), -1 = none
    bool promptWholeLevel = false; // prompt said "whole level"
    bool fromAI = false;

    int& operator[](int m) { return mods[m]; }
    int operator[](int m) const { return mods[m]; }
};

// What the user picked in the menu.
struct UserChoices {
    Tier tier = Tier::Epic;
    bool wholeLevel = false;
    int fill = 0;                       // 0 Auto, 1 Off, 2+ = FillStyle + 2
    std::array<int, kUserModuleCount> overrides {}; // 0 Auto, 1 On, 2 Off
    float density = 1.f;
};

namespace genesis {
    // Instant offline generator: reads the prompt for themes, colors and keywords.
    DecoPlan planFromPrompt(std::string const& prompt);

    // Reads Claude's JSON reply into a plan. Returns false if it can't be read.
    bool planFromJson(std::string const& text, DecoPlan& out);

    // Applies the quality tier, the menu choices and density.
    void finalizePlan(DecoPlan& plan, UserChoices const& choices);

    // System prompt sent to Claude.
    std::string aiSystemPrompt();

    // Saved menu choices.
    UserChoices loadChoices();
    void saveChoices(UserChoices const& c);
}
