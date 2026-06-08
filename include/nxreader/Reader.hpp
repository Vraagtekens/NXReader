#pragma once

#include "nxreader/Epub.hpp"
#include "nxreader/Settings.hpp"

#include <string>
#include <vector>

namespace nxreader {

struct SelectionRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Annotation {
    int page = 1;
    std::string text;
    std::string translation;
    std::string note;
};

struct ReaderState {
    int page = 1;
    bool darkMode = true;
    std::string bookName = "Hello, World.epub";
    std::string bookPath;
    std::string loadError;
    EpubImage coverImage;
    std::string selectedText;
    int selectedX = 0;
    int selectedY = 0;
    int selectedW = 0;
    int selectedH = 0;
    int selectionAnchor = -1;
    int selectionFocus = -1;
    std::vector<SelectionRect> selectedRects;
    std::vector<Annotation> annotations;
    int selectedAnnotation = 0;
    int annotationScroll = 0;
    std::vector<EpubImage> images;
    std::vector<std::string> chapterTexts;
    std::vector<std::string> pages;
};

ReaderState makeReaderState();
void loadReaderBook(ReaderState& state, const EpubBook& book, const std::string& fallbackName, const AppSettings& settings);
void loadReaderError(ReaderState& state, const std::string& bookName, const std::string& path, const std::string& error);
void repaginateReader(ReaderState& state, const AppSettings& settings);
void nextPage(ReaderState& state);
void previousPage(ReaderState& state);
void drawReader(const ReaderState& state);

}  // namespace nxreader
