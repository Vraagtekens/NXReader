#pragma once

#include <string>
#include <vector>

namespace nxreader {

struct BrowserEntry {
    std::string name;
    std::string path;
    std::string title;
    std::vector<unsigned char> coverBytes;
    long long size = 0;
    bool directory = false;
    bool parent = false;
};

struct BrowserState {
    std::string currentDir;
    std::vector<BrowserEntry> entries;
    int selected = 0;
    int scroll = 0;
    int visibleFiles = 0;
    int hiddenFiles = 0;
    int visibleDirs = 0;
    bool gridView = true;
    std::string message;
};

BrowserState makeBrowserState();
void scanBookDir(BrowserState &state);
void loadBrowserCovers(BrowserState &state);
void clampBrowserSelection(BrowserState &state);
void drawBrowser(const BrowserState &state);
void enterParentDirectory(BrowserState &state);

} // namespace nxreader
