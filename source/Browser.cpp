#include "nxreader/Browser.hpp"

#include "nxreader/Constants.hpp"
#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <switch.h>

namespace nxreader {
namespace {

bool readPathInfo(const std::string& path, bool& directory, long long& size) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    directory = S_ISDIR(info.st_mode);
    size = static_cast<long long>(info.st_size);
    return true;
}

std::string parentDir(const std::string& path) {
    if (path == kBooksRoot) {
        return path;
    }

    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash <= std::strlen(kBooksRoot)) {
        return kBooksRoot;
    }

    return path.substr(0, slash);
}

}  // namespace

BrowserState makeBrowserState() {
    BrowserState state;
    state.currentDir = kBooksRoot;
    return state;
}

void clampBrowserSelection(BrowserState& state) {
    if (state.entries.empty()) {
        state.selected = 0;
        state.scroll = 0;
        return;
    }

    if (state.selected < 0) {
        state.selected = 0;
    }

    const int lastIndex = static_cast<int>(state.entries.size()) - 1;
    if (state.selected > lastIndex) {
        state.selected = lastIndex;
    }

    if (state.selected < state.scroll) {
        state.scroll = state.selected;
    }

    if (state.selected >= state.scroll + kVisibleRows) {
        state.scroll = state.selected - kVisibleRows + 1;
    }
}

void scanBookDir(BrowserState& state) {
    state.entries.clear();
    state.message.clear();

    DIR* dir = opendir(state.currentDir.c_str());
    if (dir == nullptr) {
        state.message = "Could not open this folder. Expected SD path: sdmc:/books";
        state.currentDir = kBooksRoot;
        state.selected = 0;
        state.scroll = 0;
        return;
    }

    if (state.currentDir != kBooksRoot) {
        state.entries.push_back({"..", parentDir(state.currentDir), 0, true, true});
    }

    while (dirent* entry = readdir(dir)) {
        const char* name = entry->d_name;
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0 || name[0] == '.') {
            continue;
        }

        const std::string path = joinPath(state.currentDir, name);
        bool directory = false;
        long long size = 0;
        if (!readPathInfo(path, directory, size)) {
            continue;
        }

        if (!directory && (!endsWithIgnoreCase(name, ".epub") || size <= 0)) {
            continue;
        }

        state.entries.push_back({name, path, size, directory, false});
    }

    closedir(dir);

    std::sort(state.entries.begin(), state.entries.end(), [](const BrowserEntry& left, const BrowserEntry& right) {
        if (left.parent != right.parent) {
            return left.parent;
        }
        if (left.directory != right.directory) {
            return left.directory > right.directory;
        }
        return lowerCopy(left.name) < lowerCopy(right.name);
    });

    if (state.entries.empty()) {
        state.message = "No .epub files found here. Put books in sdmc:/books.";
    }

    clampBrowserSelection(state);
}

void enterParentDirectory(BrowserState& state) {
    if (state.currentDir == kBooksRoot) {
        return;
    }

    state.currentDir = parentDir(state.currentDir);
    state.selected = 0;
    state.scroll = 0;
    scanBookDir(state);
}

void drawBrowser(const BrowserState& state) {
    consoleClear();

    std::printf("NXReader - Books\n");
    std::printf("================\n\n");
    std::printf("Folder: %s\n\n", state.currentDir.c_str());

    if (!state.message.empty()) {
        std::printf("%s\n\n", state.message.c_str());
    }

    const int visibleEnd = std::min(static_cast<int>(state.entries.size()), state.scroll + kVisibleRows);
    for (int index = state.scroll; index < visibleEnd; ++index) {
        const BrowserEntry& entry = state.entries[index];
        const char* cursor = index == state.selected ? ">" : " ";
        const char* kind = entry.directory ? "[DIR] " : "      ";

        if (entry.directory) {
            std::printf("%s %s%s\n", cursor, kind, entry.name.c_str());
        } else {
            std::printf("%s %s%s (%lld KB)\n", cursor, kind, entry.name.c_str(), entry.size / 1024);
        }
    }

    if (state.entries.size() > kVisibleRows) {
        std::printf("\nShowing %d-%d of %lu\n", state.scroll + 1, visibleEnd, state.entries.size());
    }

    std::printf("\nControls\n");
    std::printf("D-Pad Up/Down    Move\n");
    std::printf("A                Open folder/book\n");
    std::printf("B                Parent folder\n");
    std::printf("Y                Refresh\n");
    std::printf("+                Exit\n");

    consoleUpdate(nullptr);
}

}  // namespace nxreader
