#include "nxreader/Renderer.hpp"

#include "nxreader/Constants.hpp"
#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace nxreader {
namespace {

constexpr int kScreenHeight = 720;
constexpr int kMargin = 46;
constexpr int kReaderChromeTop = 122;
constexpr int kReaderFocusTop = 40;
constexpr int kReaderBottom = 660;
constexpr const char* kDefaultFontPath = "romfs:/font.ttf";

SDL_Color rgb(unsigned char r, unsigned char g, unsigned char b) {
    return SDL_Color{r, g, b, 255};
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

std::string filenameFromPath(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

void removeLastUtf8Char(std::string& text) {
    if (text.empty()) {
        return;
    }

    text.pop_back();
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xc0) == 0x80) {
        text.pop_back();
    }
}

std::string fitSingleLine(TTF_Font* font, const std::string& text, int maxWidth) {
    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &width, &height) == 0 && width <= maxWidth) {
        return text;
    }

    std::string fitted = text;
    constexpr const char* ellipsis = "...";
    while (!fitted.empty()) {
        removeLastUtf8Char(fitted);
        const std::string candidate = fitted + ellipsis;
        if (TTF_SizeUTF8(font, candidate.c_str(), &width, &height) == 0 && width <= maxWidth) {
            return candidate;
        }
    }

    return ellipsis;
}

TTF_Font* tryOpenFont(const std::string& path, int fontSize, std::string* loadedPath) {
    if (path.empty()) {
        return nullptr;
    }

    TTF_Font* font = TTF_OpenFont(path.c_str(), fontSize);
    if (font != nullptr && loadedPath != nullptr) {
        *loadedPath = path;
    }
    return font;
}

TTF_Font* openConfiguredFont(int fontIndex, int fontSize, std::string* loadedPath = nullptr) {
    TTF_Font* font = tryOpenFont(settingsFontPath(fontIndex), fontSize, loadedPath);
    if (font != nullptr) {
        return font;
    }

    if (clampFontIndex(fontIndex) >= 2) {
        return tryOpenFont(kDefaultFontPath, fontSize, loadedPath);
    }
    return nullptr;
}

bool imageMarkerPath(const std::string& line, std::string& path) {
    constexpr const char* marker = "[[NXREADER_IMAGE:";
    constexpr size_t markerLength = 17;
    if (line.rfind(marker, 0) != 0) {
        return false;
    }

    const size_t end = line.find("]]", markerLength);
    if (end == std::string::npos) {
        return false;
    }

    path = line.substr(markerLength, end - markerLength);
    return true;
}

const EpubImage* findReaderImage(const ReaderState& state, const std::string& path) {
    for (const EpubImage& image : state.images) {
        if (image.href == path) {
            return &image;
        }
    }
    return nullptr;
}

}  // namespace

bool Renderer::init(std::string& error) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        error = SDL_GetError();
        return false;
    }

    if (TTF_Init() != 0) {
        error = TTF_GetError();
        SDL_Quit();
        return false;
    }

    const int imageFlags = IMG_INIT_JPG | IMG_INIT_PNG;
    if ((IMG_Init(imageFlags) & imageFlags) != imageFlags) {
        error = IMG_GetError();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    window_ = SDL_CreateWindow("NXReader", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kScreenWidth, kScreenHeight, 0);
    if (window_ == nullptr) {
        error = SDL_GetError();
        shutdown();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        error = SDL_GetError();
        shutdown();
        return false;
    }

    if (!applySettings(AppSettings{}, error)) {
        return false;
    }
    return true;
}

bool Renderer::applySettings(const AppSettings& settings, std::string& error) {
    const int fontSize = clampFontSize(settings.fontSize);
    const int fontIndex = clampFontIndex(settings.fontIndex);
    if (bodyFont_ != nullptr && titleFont_ != nullptr && smallFont_ != nullptr && uiFont_ != nullptr && uiTitleFont_ != nullptr &&
        loadedFontSize_ == fontSize && loadedFontIndex_ == fontIndex) {
        return true;
    }

    if (bodyFont_ != nullptr) {
        TTF_CloseFont(bodyFont_);
        bodyFont_ = nullptr;
    }
    if (titleFont_ != nullptr) {
        TTF_CloseFont(titleFont_);
        titleFont_ = nullptr;
    }
    if (smallFont_ != nullptr) {
        TTF_CloseFont(smallFont_);
        smallFont_ = nullptr;
    }
    if (uiFont_ != nullptr) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
    }
    if (uiTitleFont_ != nullptr) {
        TTF_CloseFont(uiTitleFont_);
        uiTitleFont_ = nullptr;
    }

    const std::string fontPath = settingsFontPath(fontIndex);
    std::string loadedFontPath;
    bodyFont_ = openConfiguredFont(fontIndex, fontSize, &loadedFontPath);
    titleFont_ = openConfiguredFont(fontIndex, fontSize + 14);
    smallFont_ = TTF_OpenFont(kDefaultFontPath, 18);
    uiFont_ = TTF_OpenFont(kDefaultFontPath, 24);
    uiTitleFont_ = TTF_OpenFont(kDefaultFontPath, 38);
    if (bodyFont_ == nullptr || titleFont_ == nullptr || smallFont_ == nullptr || uiFont_ == nullptr || uiTitleFont_ == nullptr) {
        error = std::string("Could not load font: ") + fontPath + "\n" + TTF_GetError();
        return false;
    }

    loadedFontSize_ = fontSize;
    loadedFontIndex_ = fontIndex;
    loadedFontPath_ = loadedFontPath;
    TTF_SetFontStyle(titleFont_, TTF_STYLE_BOLD);
    TTF_SetFontStyle(uiTitleFont_, TTF_STYLE_BOLD);
    return true;
}

void Renderer::shutdown() {
    if (uiTitleFont_ != nullptr) {
        TTF_CloseFont(uiTitleFont_);
        uiTitleFont_ = nullptr;
    }
    if (uiFont_ != nullptr) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
    }
    if (smallFont_ != nullptr) {
        TTF_CloseFont(smallFont_);
        smallFont_ = nullptr;
    }
    if (titleFont_ != nullptr) {
        TTF_CloseFont(titleFont_);
        titleFont_ = nullptr;
    }
    if (bodyFont_ != nullptr) {
        TTF_CloseFont(bodyFont_);
        bodyFont_ = nullptr;
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void Renderer::clear(SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

void Renderer::present() {
    SDL_RenderPresent(renderer_);
}

int Renderer::drawText(TTF_Font* font, const std::string& text, int x, int y, int wrapWidth, SDL_Color color) {
    if (text.empty()) {
        return TTF_FontHeight(font);
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color, wrapWidth);
    if (surface == nullptr) {
        return TTF_FontHeight(font);
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    const int height = surface->h;
    SDL_Rect rect{x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);

    if (texture != nullptr) {
        SDL_RenderCopy(renderer_, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }

    return height;
}

int Renderer::drawSingleLine(TTF_Font* font, const std::string& text, int x, int y, int maxWidth, SDL_Color color) {
    const std::string fitted = fitSingleLine(font, text, maxWidth);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, fitted.c_str(), color);
    if (surface == nullptr) {
        return TTF_FontHeight(font);
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    const int height = surface->h;
    SDL_Rect rect{x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);

    if (texture != nullptr) {
        SDL_RenderCopy(renderer_, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
    }

    return height;
}

void Renderer::drawHeader(const std::string& title, const std::string& subtitle) {
    drawSingleLine(uiTitleFont_, title, kMargin, 30, 760, rgb(245, 242, 232));
    drawSingleLine(smallFont_, subtitle, kMargin, 76, 900, rgb(184, 191, 199));
}

bool Renderer::drawImageBytes(const std::vector<unsigned char>& bytes, int x, int y, int maxWidth, int maxHeight) {
    if (bytes.empty()) {
        return false;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
    if (rw == nullptr) {
        return false;
    }

    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (surface == nullptr) {
        return false;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    const int width = surface->w;
    const int height = surface->h;
    SDL_FreeSurface(surface);

    if (texture == nullptr || width <= 0 || height <= 0) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
        return false;
    }

    const float scale = std::min(static_cast<float>(maxWidth) / static_cast<float>(width),
                                 static_cast<float>(maxHeight) / static_cast<float>(height));
    const int drawWidth = std::max(1, static_cast<int>(width * scale));
    const int drawHeight = std::max(1, static_cast<int>(height * scale));
    SDL_Rect rect{x + (maxWidth - drawWidth) / 2, y + (maxHeight - drawHeight) / 2, drawWidth, drawHeight};
    SDL_RenderCopy(renderer_, texture, nullptr, &rect);
    SDL_DestroyTexture(texture);
    return true;
}

void Renderer::drawBrowser(const BrowserState& state) {
    clear(rgb(18, 22, 27));
    drawHeader("NXReader", state.currentDir);

    int y = 128;
    const int visibleEnd = std::min(static_cast<int>(state.entries.size()), state.scroll + kVisibleRows);
    for (int index = state.scroll; index < visibleEnd; ++index) {
        const BrowserEntry& entry = state.entries[index];
        const bool selected = index == state.selected;

        if (selected) {
            SDL_Rect rect{kMargin - 14, y - 8, 1188, 38};
            SDL_SetRenderDrawColor(renderer_, 48, 62, 77, 255);
            SDL_RenderFillRect(renderer_, &rect);
        }

        std::string label = entry.directory ? "[DIR] " + entry.name : entry.name;
        drawSingleLine(uiFont_, label, kMargin, y, 950, selected ? rgb(255, 255, 255) : rgb(219, 223, 228));
        if (!entry.directory) {
            drawSingleLine(smallFont_, std::to_string(entry.size / 1024) + " KB", 1060, y + 5, 150,
                           selected ? rgb(235, 239, 245) : rgb(162, 171, 181));
        }
        y += 42;
    }

    if (!state.message.empty()) {
        drawText(smallFont_, state.message, kMargin, y + 16, 1080, rgb(240, 190, 120));
    }

    const std::string footer = "Books: " + std::to_string(state.visibleFiles) + "    Folders: " +
                               std::to_string(state.visibleDirs) + "    Hidden: " + std::to_string(state.hiddenFiles) +
                               "    A open    B parent    Y refresh    + exit";
    drawSingleLine(smallFont_, footer, kMargin, 672, 1160, rgb(162, 171, 181));
    present();
}

void Renderer::drawReader(const ReaderState& state, const AppSettings& settings, bool showSettings, bool showChrome) {
    clear(state.darkMode ? rgb(14, 17, 21) : rgb(236, 232, 220));

    const SDL_Color body = state.darkMode ? rgb(238, 235, 225) : rgb(25, 28, 33);
    const SDL_Color muted = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);

    if (showChrome || showSettings) {
        drawText(smallFont_, state.bookName, kMargin, 24, 820, muted);
        drawText(smallFont_, "Page " + std::to_string(state.page) + " / " + std::to_string(std::max<int>(1, state.pages.size())),
                 kMargin, 54, 820, muted);
    }

    const int index = std::max(0, std::min(state.page - 1, static_cast<int>(state.pages.size()) - 1));
    if (state.pages[index] == "[[NXREADER_COVER]]") {
        const bool drewCover = drawImageBytes(state.coverImage.bytes, 170, 100, 940, 520);
        if (!drewCover) {
            drawText(titleFont_, "Cover image", kMargin, 220, 900, body);
            drawText(bodyFont_, "This EPUB has a cover, but NXReader could not render its image format yet.",
                     kMargin, 288, 1040, body);
        }
    } else {
        const std::vector<std::string> lines = splitLines(state.pages[index]);
        int y = (showChrome || showSettings) ? kReaderChromeTop : kReaderFocusTop;

        if (!state.loadError.empty()) {
            drawText(titleFont_, "EPUB load error", kMargin, y, 880, rgb(255, 180, 120));
            y += 58;
        }

        for (const std::string& rawLine : lines) {
            if (y > kReaderBottom) {
                break;
            }

            if (rawLine.empty()) {
                y += 8;
                continue;
            }

            std::string imagePath;
            if (imageMarkerPath(rawLine, imagePath)) {
                const EpubImage* image = findReaderImage(state, imagePath);
                if (image != nullptr && drawImageBytes(image->bytes, kMargin, y, 1040, 360)) {
                    y += 380;
                } else {
                    drawText(bodyFont_, "[Image]", kMargin, y, 1040, body);
                    y += TTF_FontHeight(bodyFont_) + 8;
                }
                continue;
            }

            const bool heading = rawLine.rfind("## ", 0) == 0;
            const std::string line = heading ? rawLine.substr(3) : rawLine;
            TTF_Font* font = heading ? titleFont_ : bodyFont_;
            const SDL_Color color = body;
            const int height = drawText(font, line, kMargin, y, 1040, color);
            y += height + (heading ? 16 : 4);
        }
    }

    if (showSettings) {
        SDL_Rect panel{300, 118, 680, 438};
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 32 : 246, state.darkMode ? 37 : 243, state.darkMode ? 45 : 235, 245);
        SDL_RenderFillRect(renderer_, &panel);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 88 : 170, state.darkMode ? 96 : 166, state.darkMode ? 108 : 156, 255);
        SDL_RenderDrawRect(renderer_, &panel);

        const SDL_Color panelText = state.darkMode ? rgb(245, 242, 232) : rgb(25, 28, 33);
        const SDL_Color selected = state.darkMode ? rgb(154, 201, 255) : rgb(30, 82, 145);
        const std::string fontName = settingsFontName(settings.fontIndex);
        drawSingleLine(uiTitleFont_, "Settings", 344, 168, 590, panelText);
        drawSingleLine(uiFont_, std::string(settings.selectedSetting == 0 ? "> " : "  ") + "Font: " + fontName,
                       344, 246, 590, settings.selectedSetting == 0 ? selected : panelText);
        drawSingleLine(uiFont_, std::string(settings.selectedSetting == 1 ? "> " : "  ") + "Size: " + std::to_string(settings.fontSize),
                       344, 300, 590, settings.selectedSetting == 1 ? selected : panelText);
        drawSingleLine(uiFont_, std::string(settings.selectedSetting == 2 ? "> " : "  ") + "Header on turn: " +
                                    (settings.showHeaderOnTurn ? "On" : "Off"),
                       344, 354, 590, settings.selectedSetting == 2 ? selected : panelText);
        drawSingleLine(smallFont_, "Up/Down selects    Left/Right changes    Y closes", 344, 444, 590, panelText);
        const std::string loadedFont = loadedFontPath_.empty() ? "unknown" : filenameFromPath(loadedFontPath_);
        drawSingleLine(smallFont_, "Loaded: " + loadedFont, 344, 480, 590, panelText);
    }
    present();
}

}  // namespace nxreader
