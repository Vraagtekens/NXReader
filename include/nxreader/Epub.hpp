#pragma once

#include <string>
#include <vector>

namespace nxreader {

struct EpubChapter {
    std::string href;
    std::string mediaType;
    std::string text;
};

struct EpubImage {
    std::string href;
    std::string mediaType;
    std::vector<unsigned char> bytes;
};

struct EpubBook {
    std::string path;
    std::string title;
    EpubImage coverImage;
    std::vector<EpubImage> images;
    std::vector<EpubChapter> chapters;
};

bool loadEpub(const std::string& path, EpubBook& book, std::string& error);

}  // namespace nxreader
