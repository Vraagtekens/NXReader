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
};

int clampFontSize(int fontSize);
int clampFontIndex(int fontIndex);
int settingsFontCount();
std::string settingsFontName(int fontIndex);
std::string settingsFontPath(int fontIndex);

}  // namespace nxreader
