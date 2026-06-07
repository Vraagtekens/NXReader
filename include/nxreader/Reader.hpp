#pragma once

#include "nxreader/Epub.hpp"
#include "nxreader/Settings.hpp"

#include <string>
#include <vector>

namespace nxreader {

struct ReaderState {
    int page = 1;
    bool darkMode = true;
    std::string bookName = "Hello, World.epub";
    std::string bookPath;
    std::string loadError;
    EpubImage coverImage;
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
