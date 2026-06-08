#include "nxreader/Reader.hpp"

#include "nxreader/Storage.hpp"
#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <switch.h>

namespace nxreader {
namespace {

constexpr int kReaderColumns = 72;
constexpr const char *kCoverPageMarker = "[[NXREADER_COVER]]";
constexpr const char *kImagePageMarker = "[[NXREADER_IMAGE:";

int pageCount(const ReaderState &state) {
    return std::max(1, static_cast<int>(state.pages.size()));
}

int linesPerPageForSettings(const AppSettings &settings) {
    if (settings.fontSize >= 36) {
        return 8;
    }
    if (settings.fontSize >= 32) {
        return 10;
    }
    if (settings.fontSize >= 28) {
        return 12;
    }
    if (settings.fontSize >= 24) {
        return 14;
    }
    return 15;
}

int columnsForSettings(const AppSettings &settings) {
    return std::max(48, kReaderColumns - (settings.fontSize - 28));
}

void clearSelection(ReaderState &state) {
    state.selectedText.clear();
    state.selectedX = 0;
    state.selectedY = 0;
    state.selectedW = 0;
    state.selectedH = 0;
    state.selectionAnchor = -1;
    state.selectionFocus = -1;
    state.selectedRects.clear();
}

std::vector<std::string> paginateForViewport(const std::string &text, const AppSettings &settings) {
    const std::vector<std::string> lines = wrapTextLines(text, columnsForSettings(settings));
    std::vector<std::string> pages;
    std::string page;
    int usedLines = 0;
    const int linesPerPage = linesPerPageForSettings(settings);

    for (const std::string &line : lines) {
        const bool heading = line.rfind("## ", 0) == 0;
        const bool image = line.rfind(kImagePageMarker, 0) == 0;
        const int lineCost = image ? 10 : (line.empty() ? 1 : (heading ? 2 : 1));

        if (usedLines > 0 && usedLines + lineCost > linesPerPage) {
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

} // namespace

ReaderState makeReaderState() {
    ReaderState state;
    state.pages.push_back("Select an EPUB file from sdmc:/switch/NXReader/books to begin.");
    return state;
}

void loadReaderBook(ReaderState &state, const EpubBook &book, const std::string &fallbackName,
                    const AppSettings &settings) {
    state.bookName = book.title.empty() ? fallbackName : book.title;
    state.bookPath = book.path;
    state.darkMode = settings.darkMode;
    state.loadError.clear();
    state.coverImage = book.coverImage;
    clearSelection(state);
    state.annotations.clear();
    state.selectedAnnotation = 0;
    state.annotationScroll = 0;
    state.images = book.images;
    state.chapterTexts.clear();
    state.pages.clear();

    for (const EpubChapter &chapter : book.chapters) {
        state.chapterTexts.push_back(chapter.text);
    }

    repaginateReader(state, settings);
    state.page = loadLastPage(state.bookPath.c_str(), pageCount(state));
    state.annotations = loadAnnotations(state.bookPath.c_str());
}

void loadReaderError(ReaderState &state, const std::string &bookName, const std::string &path,
                     const std::string &error) {
    state.bookName = bookName;
    state.bookPath = path;
    state.loadError = error;
    state.coverImage = {};
    clearSelection(state);
    state.annotations.clear();
    state.selectedAnnotation = 0;
    state.annotationScroll = 0;
    state.images.clear();
    state.chapterTexts.clear();
    state.pages.clear();
    state.pages.push_back(consoleSafeText(error));
    state.page = 1;
}

void repaginateReader(ReaderState &state, const AppSettings &settings) {
    const int oldPage = state.page;
    std::vector<std::string> allPages;

    for (const std::string &chapterText : state.chapterTexts) {
        std::vector<std::string> chapterPages = paginateForViewport(chapterText, settings);
        allPages.insert(allPages.end(), chapterPages.begin(), chapterPages.end());
    }

    state.pages = allPages.empty() ? paginateForViewport("This book has no readable text yet.", settings) : allPages;
    if (!state.coverImage.bytes.empty()) {
        state.pages.insert(state.pages.begin(), kCoverPageMarker);
    }
    state.page = std::max(1, std::min(oldPage, pageCount(state)));
}

void nextPage(ReaderState &state) {
    if (state.page < pageCount(state)) {
        state.page += 1;
        clearSelection(state);
        saveLastPage(state.bookPath.c_str(), state.page);
    }
}

void previousPage(ReaderState &state) {
    if (state.page > 1) {
        state.page -= 1;
        clearSelection(state);
        saveLastPage(state.bookPath.c_str(), state.page);
    }
}

void drawReader(const ReaderState &state) {
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
    std::printf("Touch a word to select it\n");

    consoleUpdate(nullptr);
}

} // namespace nxreader
