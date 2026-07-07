#pragma once

#include <string>

namespace nxreader {

struct AppSettings {
    int fontSize = 28;
    int fontIndex = 0;
    int selectedSetting = 0;
    bool darkMode = true;
    bool showHeaderOnTurn = true;
    bool animatePageTurns = true;
    bool showPageCounter = true;
    bool browserGridView = false;
};

int clampFontSize(int fontSize);
int clampFontIndex(int fontIndex);
int settingsFontCount();
std::string settingsFontName(int fontIndex);
std::string settingsFontPath(int fontIndex);
std::string settingsFontBoldPath(int fontIndex);
std::string settingsFontItalicPath(int fontIndex);
std::string settingsFontBoldItalicPath(int fontIndex);

} // namespace nxreader
