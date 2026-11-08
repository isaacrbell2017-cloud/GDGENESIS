#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include "Plan.hpp"

namespace genesis {
    struct GenerateResult {
        bool ok = false;
        int created = 0;
        std::string error;
    };

    // Number of objects currently selected in the editor (0 if not in the editor).
    int selectedCount();

    // Builds the deco around the current selection and adds it to the level.
    GenerateResult generate(LevelEditorLayer* editor, DecoPlan const& plan);
}
