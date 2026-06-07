#include "nxreader/Storage.hpp"

#include "nxreader/Constants.hpp"
#include "nxreader/Settings.hpp"

#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace nxreader {

void ensureSaveDirExists() {
    mkdir("sdmc:/switch", 0777);
    mkdir(kSaveDir, 0777);
    mkdir(kFontsRoot, 0777);
}

namespace {

unsigned int fnv1a(const char* value) {
    unsigned int hash = 2166136261u;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value); *cursor != 0; ++cursor) {
        hash ^= *cursor;
        hash *= 16777619u;
    }
    return hash;
}

std::string progressPath(const char* bookPath) {
    char path[128] = {};
    std::snprintf(path, sizeof(path), "%s/progress_%08x.txt", kSaveDir, fnv1a(bookPath));
    return path;
}

}  // namespace

int loadLastPage(const char* bookPath, int maxPage) {
    const std::string path = progressPath(bookPath == nullptr ? "" : bookPath);
    FILE* file = std::fopen(path.c_str(), "r");
    if (file == nullptr) {
        return 1;
    }

    int page = 1;
    if (std::fscanf(file, "%d", &page) != 1) {
        page = 1;
    }

    std::fclose(file);

    if (page < 1) {
        return 1;
    }

    if (page > maxPage) {
        return maxPage;
    }

    return page;
}

void saveLastPage(const char* bookPath, int page) {
    ensureSaveDirExists();

    const std::string path = progressPath(bookPath == nullptr ? "" : bookPath);
    FILE* file = std::fopen(path.c_str(), "w");
    if (file == nullptr) {
        return;
    }

    std::fprintf(file, "%d\n%s\n", page, bookPath == nullptr ? "" : bookPath);
    std::fclose(file);
}

AppSettings loadSettings() {
    AppSettings settings;
    FILE* file = std::fopen("sdmc:/switch/NXReader/settings.txt", "r");
    if (file == nullptr) {
        return settings;
    }

    int fontSize = settings.fontSize;
    int fontIndex = settings.fontIndex;
    int showHeaderOnTurn = settings.showHeaderOnTurn ? 1 : 0;
    std::fscanf(file, "fontSize=%d\nfontIndex=%d\nshowHeaderOnTurn=%d", &fontSize, &fontIndex, &showHeaderOnTurn);
    std::fclose(file);

    settings.fontSize = clampFontSize(fontSize);
    settings.fontIndex = clampFontIndex(fontIndex);
    settings.showHeaderOnTurn = showHeaderOnTurn != 0;
    return settings;
}

void saveSettings(const AppSettings& settings) {
    ensureSaveDirExists();

    FILE* file = std::fopen("sdmc:/switch/NXReader/settings.txt", "w");
    if (file == nullptr) {
        return;
    }

    std::fprintf(file, "fontSize=%d\nfontIndex=%d\nshowHeaderOnTurn=%d\n",
                 clampFontSize(settings.fontSize),
                 clampFontIndex(settings.fontIndex),
                 settings.showHeaderOnTurn ? 1 : 0);
    std::fclose(file);
}

}  // namespace nxreader
