#include "nxreader/Settings.hpp"

#include "nxreader/Constants.hpp"

#include <algorithm>
#include <cstdio>
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

struct BundledFont {
    const char *name;
    const char *path;
    const char *boldPath;
};

constexpr BundledFont kBundledFonts[] = {
    {"Atkinson Hyperlegible",
     "romfs:/fonts/AtkinsonHyperlegible/AtkinsonHyperlegible-Regular.ttf",
     "romfs:/fonts/AtkinsonHyperlegible/AtkinsonHyperlegible-Bold.ttf"},
    {"Lexend", "romfs:/fonts/Lexend/Lexend-Regular.ttf", "romfs:/fonts/Lexend/Lexend-Bold.ttf"},
    {"OpenDyslexic",
     "romfs:/fonts/OpenDyslexic/OpenDyslexic-Regular.ttf",
     "romfs:/fonts/OpenDyslexic/OpenDyslexic-Bold.ttf"},
};
constexpr int kBundledFontCount = sizeof(kBundledFonts) / sizeof(kBundledFonts[0]);

bool hasFontExtension(const char *name, const char *extension) {
    const size_t nameLength = std::strlen(name);
    const size_t extensionLength = std::strlen(extension);
    if (nameLength < extensionLength) {
        return false;
    }

    return strcasecmp(name + nameLength - extensionLength, extension) == 0;
}

void collectFonts(const std::string &directory, int depth, std::vector<std::string> &preferredFonts,
                  std::vector<std::string> &fallbackFonts) {
    if (depth > 3) {
        return;
    }

    DIR *dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return;
    }

    while (dirent *entry = readdir(dir)) {
        const char *name = entry->d_name;
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

std::string filenameFromPath(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string fontNameFromPath(const std::string &path) {
    std::string name = filenameFromPath(path);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        name = name.substr(0, dot);
    }
    return name;
}

} // namespace

int settingsFontCount() {
    return kBundledFontCount + static_cast<int>(customFontPaths().size());
}

std::string settingsFontName(int fontIndex) {
    const int clampedIndex = clampFontIndex(fontIndex);
    if (clampedIndex < kBundledFontCount) {
        return kBundledFonts[clampedIndex].name;
    }

    const std::vector<std::string> customFonts = customFontPaths();
    const int customIndex = clampedIndex - kBundledFontCount;
    if (customIndex >= 0 && customIndex < static_cast<int>(customFonts.size())) {
        return fontNameFromPath(customFonts[customIndex]);
    }
    return "Custom Font";
}

std::string settingsFontPath(int fontIndex) {
    const int clampedIndex = clampFontIndex(fontIndex);
    if (clampedIndex < kBundledFontCount) {
        return kBundledFonts[clampedIndex].path;
    }

    const std::vector<std::string> customFonts = customFontPaths();
    const int customIndex = clampedIndex - kBundledFontCount;
    if (customIndex >= 0 && customIndex < static_cast<int>(customFonts.size())) {
        return customFonts[customIndex];
    }
    return "romfs:/font.ttf";
}

std::string settingsFontBoldPath(int fontIndex) {
    const int clampedIndex = clampFontIndex(fontIndex);
    if (clampedIndex < kBundledFontCount) {
        return kBundledFonts[clampedIndex].boldPath;
    }

    const std::string regularPath = settingsFontPath(fontIndex);
    const size_t dot = regularPath.find_last_of('.');
    const std::string stem = dot == std::string::npos ? regularPath : regularPath.substr(0, dot);
    const std::string extension = dot == std::string::npos ? "" : regularPath.substr(dot);

    const std::string candidates[] = {
        stem + "-Bold" + extension,
        stem + "-bold" + extension,
        stem + " Bold" + extension,
        stem + "_Bold" + extension,
        stem + "_bold" + extension,
    };

    for (const std::string &candidate : candidates) {
        FILE *file = std::fopen(candidate.c_str(), "rb");
        if (file != nullptr) {
            std::fclose(file);
            return candidate;
        }
    }
    return regularPath;
}

} // namespace nxreader
