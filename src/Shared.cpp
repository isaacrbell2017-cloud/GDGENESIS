#include "Shared.hpp"
#include "GenesisPopup.hpp"

#include <algorithm>
#include <cctype>

using namespace geode::prelude;

namespace genesis {

void sfx(char const* file) {
    if (!Mod::get()->getSettingValue<bool>("button-sfx")) return;
    if (auto engine = FMODAudioEngine::sharedEngine()) {
        engine->playEffect(file);
    }
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string trim(std::string const& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool isAllowed() {
    if (!Mod::get()->getSettingValue<bool>("owner-lock")) return true;
    auto am = GJAccountManager::sharedState();
    if (!am) return true;
    std::string name = am->m_username;
    if (name.empty()) return true; // logged out
    return toLower(name) == "litchin";
}

void openGenerator(std::function<void()> onDone) {
    if (GenesisPopup::isOpen()) return;
    if (!isAllowed()) {
        sfx("quitSound_01.ogg");
        FLAlertLayer::create(
            "GdGenesis",
            "GdGenesis is locked to <cg>Litchin</c> for now.",
            "OK"
        )->show();
        return;
    }
    if (auto popup = GenesisPopup::create(std::move(onDone))) {
        popup->show();
    }
}

} // namespace genesis
