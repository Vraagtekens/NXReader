#pragma once

#include "nxreader/Browser.hpp"
#include "nxreader/Reader.hpp"
#include "nxreader/Settings.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

namespace nxreader {

class Renderer {
public:
    bool init(std::string& error);
    bool applySettings(const AppSettings& settings, std::string& error);
    void shutdown();
    void drawBrowser(const BrowserState& state);
    void drawReader(const ReaderState& state, const AppSettings& settings, bool showSettings, bool showChrome);

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* bodyFont_ = nullptr;
    TTF_Font* titleFont_ = nullptr;
    TTF_Font* smallFont_ = nullptr;
    TTF_Font* uiFont_ = nullptr;
    TTF_Font* uiTitleFont_ = nullptr;
    int loadedFontSize_ = 0;
    int loadedFontIndex_ = -1;
    std::string loadedFontPath_;

    void clear(SDL_Color color);
    void present();
    int drawText(TTF_Font* font, const std::string& text, int x, int y, int wrapWidth, SDL_Color color);
    int drawSingleLine(TTF_Font* font, const std::string& text, int x, int y, int maxWidth, SDL_Color color);
    void drawHeader(const std::string& title, const std::string& subtitle);
    bool drawImageBytes(const std::vector<unsigned char>& bytes, int x, int y, int maxWidth, int maxHeight);
};

}  // namespace nxreader
