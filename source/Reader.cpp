#include "nxreader/Reader.hpp"

#include "nxreader/Storage.hpp"
#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <switch.h>

namespace nxreader {
namespace {

constexpr int kReaderColumns = 72;
constexpr int kReaderLinesPerPage = 18;

int pageCount(const ReaderState& state) {
    return std::max(1, static_cast<int>(state.pages.size()));
}

std::vector<std::string> paginateForViewport(const std::string& text) {
    const std::vector<std::string> lines = wrapTextLines(text, kReaderColumns);
    std::vector<std::string> pages;
    std::string page;
    int usedLines = 0;

    for (const std::string& line : lines) {
        const bool heading = line.rfind("## ", 0) == 0;
        const int lineCost = line.empty() ? 1 : (heading ? 2 : 1);

        if (usedLines > 0 && usedLines + lineCost > kReaderLinesPerPage) {
            pages.push_back(page);
            page.clear();
            usedLines = 0;
        }

        page += line;
        page += '\n';
        usedLines += lineCost;
    }

    if (!page.empty()) {
        pages.push_back(page);
    }

    if (pages.empty()) {
        pages.push_back("This page has no readable text yet.");
    }

    return pages;
}

}  // namespace

ReaderState makeReaderState() {
    ReaderState state;
    state.pages.push_back("Select an EPUB file from sdmc:/books to begin.");
    return state;
}

void loadReaderBook(ReaderState& state, const EpubBook& book, const std::string& fallbackName) {
    state.bookName = book.title.empty() ? fallbackName : book.title;
    state.bookPath = book.path;
    state.loadError.clear();
    state.pages.clear();

    std::string combinedText;
    for (const EpubChapter& chapter : book.chapters) {
        combinedText += chapter.text;
        combinedText += "\n\n";
    }

    state.pages = paginateForViewport(combinedText);
    state.page = loadLastPage(state.bookPath.c_str(), pageCount(state));
}

void loadReaderError(ReaderState& state, const std::string& bookName, const std::string& path, const std::string& error) {
    state.bookName = bookName;
    state.bookPath = path;
    state.loadError = error;
    state.pages.clear();
    state.pages.push_back(consoleSafeText(error));
    state.page = 1;
}

void nextPage(ReaderState& state) {
    if (state.page < pageCount(state)) {
        state.page += 1;
        saveLastPage(state.bookPath.c_str(), state.page);
    }
}

void previousPage(ReaderState& state) {
    if (state.page > 1) {
        state.page -= 1;
        saveLastPage(state.bookPath.c_str(), state.page);
    }
}

void drawReader(const ReaderState& state) {
    consoleClear();

    std::printf("NXReader - Reading\n");
    std::printf("==================\n\n");
    std::printf("Book: %s\n", state.bookName.c_str());
    std::printf("Page: %d / %d\n", state.page, pageCount(state));
    std::printf("Theme: %s\n\n", state.darkMode ? "Dark" : "Light");

    if (!state.loadError.empty()) {
        std::printf("EPUB load error\n\n");
        std::printf("Path:\n%s\n\n", state.bookPath.c_str());
    }

    const int index = std::max(0, std::min(state.page - 1, pageCount(state) - 1));
    std::printf("%s\n", state.pages[index].c_str());

    std::printf("\n\nControls\n");
    std::printf("A / D-Pad Right  Next page\n");
    std::printf("B / D-Pad Left   Previous page\n");
    std::printf("X                Toggle dark mode\n");
    std::printf("Minus            Back to browser\n");
    std::printf("+                Exit\n");
    std::printf("Touch left/right side to turn pages\n");

    consoleUpdate(nullptr);
}

}  // namespace nxreader
