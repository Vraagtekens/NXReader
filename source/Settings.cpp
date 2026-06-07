#include "nxreader/Settings.hpp"

#include "nxreader/Constants.hpp"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <vector>

namespace nxreader {

int clampFontSize(int fontSize) {
    return std::max(20, std::min(fontSize, 40));
}

int clampFontIndex(int fontIndex) {
    return std::max(0, std::min(fontIndex, settingsFontCount() - 1));
}

namespace {

bool hasFontExtension(const char* name, const char* extension) {
    const size_t nameLength = std::strlen(name);
    const size_t extensionLength = std::strlen(extension);
    if (nameLength < extensionLength) {
        return false;
    }

    return strcasecmp(name + nameLength - extensionLength, extension) == 0;
}

void collectFonts(const std::string& directory, int depth, std::vector<std::string>& preferredFonts, std::vector<std::string>& fallbackFonts) {
    if (depth > 3) {
        return;
    }

    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return;
    }

    while (dirent* entry = readdir(dir)) {
        const char* name = entry->d_name;
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0 || name[0] == '.') {
            continue;
        }

        const std::string path = directory + "/" + name;
        if (hasFontExtension(name, ".ttf") || hasFontExtension(name, ".otf")) {
            preferredFonts.push_back(path);
        } else if (hasFontExtension(name, ".woff2")) {
            fallbackFonts.push_back(path);
        } else {
            collectFonts(path, depth + 1, preferredFonts, fallbackFonts);
        }
    }
    closedir(dir);
}

std::vector<std::string> customFontPaths() {
    std::vector<std::string> preferredFonts;
    std::vector<std::string> fallbackFonts;
    collectFonts(kFontsRoot, 0, preferredFonts, fallbackFonts);

    std::sort(preferredFonts.begin(), preferredFonts.end());
    std::sort(fallbackFonts.begin(), fallbackFonts.end());
    preferredFonts.insert(preferredFonts.end(), fallbackFonts.begin(), fallbackFonts.end());
    return preferredFonts;
}

std::string filenameFromPath(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string fontNameFromPath(const std::string& path) {
    std::string name = filenameFromPath(path);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }
    return name;
}

}  // namespace

int settingsFontCount() {
    return 2 + static_cast<int>(customFontPaths().size());
}

std::string settingsFontName(int fontIndex) {
    switch (clampFontIndex(fontIndex)) {
        case 1:
            return "Noto Serif";
        case 0:
            return "Noto Sans";
        default:
            break;
    }

    const std::vector<std::string> customFonts = customFontPaths();
    const int customIndex = clampFontIndex(fontIndex) - 2;
    if (customIndex >= 0 && customIndex < static_cast<int>(customFonts.size())) {
        return fontNameFromPath(customFonts[customIndex]);
    }
    return "Custom Font";
}

std::string settingsFontPath(int fontIndex) {
    switch (clampFontIndex(fontIndex)) {
        case 1:
            return "romfs:/serif.ttf";
        case 0:
            return "romfs:/font.ttf";
        default:
            break;
    }

    const std::vector<std::string> customFonts = customFontPaths();
    const int customIndex = clampFontIndex(fontIndex) - 2;
    if (customIndex >= 0 && customIndex < static_cast<int>(customFonts.size())) {
        return customFonts[customIndex];
    }
    return "romfs:/font.ttf";
}

}  // namespace nxreader
