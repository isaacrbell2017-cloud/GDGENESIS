#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include "Plan.hpp"

namespace genesis {
    struct GenerateResult {
        bool ok = false;
        int created = 0;
        int restyled = 0; // layout blocks recolored
        int pulses = 0;   // beat pulse triggers
        std::string error;
    };

    // Number of objects currently selected in the editor (0 if not in the editor).
    int selectedCount();

    // Number of solid blocks in the whole level.
    int levelBlockCount();

    // Builds the deco around the selection (or the whole level) and adds it to the level.
    GenerateResult generate(LevelEditorLayer* editor, DecoPlan const& plan, bool wholeLevel);
}
