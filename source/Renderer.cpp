#include "nxreader/Renderer.hpp"

#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace nxreader {
namespace {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;
constexpr int kMargin = 46;
constexpr int kReaderTop = 122;
constexpr int kReaderBottom = 638;
constexpr const char* kFontPath = "romfs:/font.ttf";

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

    bodyFont_ = TTF_OpenFont(kFontPath, 24);
    titleFont_ = TTF_OpenFont(kFontPath, 38);
    smallFont_ = TTF_OpenFont(kFontPath, 18);
    if (bodyFont_ == nullptr || titleFont_ == nullptr || smallFont_ == nullptr) {
        error = std::string("Could not load font: ") + kFontPath + "\n" + TTF_GetError();
        shutdown();
        return false;
    }

    TTF_SetFontStyle(titleFont_, TTF_STYLE_BOLD);
    return true;
}

void Renderer::shutdown() {
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

void Renderer::drawHeader(const std::string& title, const std::string& subtitle) {
    drawText(titleFont_, title, kMargin, 30, 760, rgb(245, 242, 232));
    drawText(smallFont_, subtitle, kMargin, 76, 900, rgb(184, 191, 199));
}

void Renderer::drawBrowser(const BrowserState& state) {
    clear(rgb(18, 22, 27));
    drawHeader("NXReader", state.currentDir);

    int y = 128;
    const int visibleEnd = std::min(static_cast<int>(state.entries.size()), state.scroll + 13);
    for (int index = state.scroll; index < visibleEnd; ++index) {
        const BrowserEntry& entry = state.entries[index];
        const bool selected = index == state.selected;

        if (selected) {
            SDL_Rect rect{kMargin - 14, y - 8, 1188, 38};
            SDL_SetRenderDrawColor(renderer_, 48, 62, 77, 255);
            SDL_RenderFillRect(renderer_, &rect);
        }

        std::string label = entry.directory ? "[DIR] " + entry.name : entry.name + "  " + std::to_string(entry.size / 1024) + " KB";
        drawText(bodyFont_, label, kMargin, y, 1120, selected ? rgb(255, 255, 255) : rgb(219, 223, 228));
        y += 42;
    }

    if (!state.message.empty()) {
        drawText(bodyFont_, state.message, kMargin, y + 16, 1080, rgb(240, 190, 120));
    }

    drawText(smallFont_, "Up/Down move    A open    B parent    Y refresh    + exit", kMargin, 672, 1100, rgb(162, 171, 181));
    present();
}

void Renderer::drawReader(const ReaderState& state) {
    clear(state.darkMode ? rgb(14, 17, 21) : rgb(236, 232, 220));

    const SDL_Color body = state.darkMode ? rgb(238, 235, 225) : rgb(25, 28, 33);
    const SDL_Color muted = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);
    const SDL_Color accent = state.darkMode ? rgb(154, 201, 255) : rgb(30, 82, 145);

    drawText(smallFont_, state.bookName, kMargin, 24, 820, muted);
    drawText(smallFont_, "Page " + std::to_string(state.page) + " / " + std::to_string(std::max<int>(1, state.pages.size())),
             kMargin, 54, 820, muted);

    const int index = std::max(0, std::min(state.page - 1, static_cast<int>(state.pages.size()) - 1));
    const std::vector<std::string> lines = splitLines(state.pages[index]);
    int y = kReaderTop;

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

        const bool heading = rawLine.rfind("## ", 0) == 0;
        const std::string line = heading ? rawLine.substr(3) : rawLine;
        TTF_Font* font = heading ? titleFont_ : bodyFont_;
        const SDL_Color color = heading ? accent : body;
        const int height = drawText(font, line, kMargin, y, 1040, color);
        y += height + (heading ? 16 : 4);
    }

    SDL_Rect footer{0, 650, kScreenWidth, 70};
    SDL_SetRenderDrawColor(renderer_, state.darkMode ? 20 : 222, state.darkMode ? 24 : 218, state.darkMode ? 30 : 208, 255);
    SDL_RenderFillRect(renderer_, &footer);
    drawText(smallFont_, "A/Right/Down next    B/Left/Up prev    X theme    - browser    + exit",
             kMargin, 672, 1120, muted);
    present();
}

}  // namespace nxreader
