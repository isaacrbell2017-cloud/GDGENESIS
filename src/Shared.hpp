#pragma once

#include <Geode/Geode.hpp>
#include <functional>
#include <string>

namespace genesis {
    // Plays a GD sound effect if "Button Sound Effects" is on.
    void sfx(char const* file);

    // Litchin-only lock (allowed when logged in as Litchin or logged out).
    bool isAllowed();

    // Opens the generator popup. onDone runs after a successful generate.
    void openGenerator(std::function<void()> onDone);

    std::string toLower(std::string s);
    std::string trim(std::string const& s);
}
