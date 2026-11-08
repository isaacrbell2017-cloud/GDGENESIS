#pragma once

#include <Geode/Geode.hpp>
#include <string>

// A decoration "recipe". Every module is an intensity from 0 (off) to 3 (max).
struct DecoPlan {
    std::string name = "Modern Glow";
    cocos2d::ccColor3B main { 0, 200, 255 };
    cocos2d::ccColor3B accent { 255, 255, 255 };

    int lights = 2;     // light dots along exposed block edges
    int aura = 1;       // big soft orbs behind the blocks
    int particles = 2;  // small floating dots
    int chains = 0;     // chains hanging under platforms
    int grass = 0;      // grass tufts on top edges
    int bushes = 0;     // bushes peeking over top edges
    int clouds = 0;     // clouds above the build
    int rods = 0;       // small spires on top edges
    int custom = 0;     // your own deco IDs / deco found in the selection

    float scale = 1.f;
    bool fromAI = false;
};

namespace genesis {
    // Instant offline generator: reads the prompt for themes, colors and keywords.
    DecoPlan planFromPrompt(std::string const& prompt);

    // Reads Claude's JSON reply into a plan. Returns false if it can't be read.
    bool planFromJson(std::string const& text, DecoPlan& out);

    // System prompt sent to Claude.
    std::string aiSystemPrompt();
}
