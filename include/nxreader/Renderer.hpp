#pragma once

#include "nxreader/Browser.hpp"
#include "nxreader/Reader.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

namespace nxreader {

class Renderer {
public:
    bool init(std::string& error);
    void shutdown();
    void drawBrowser(const BrowserState& state);
    void drawReader(const ReaderState& state);

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* bodyFont_ = nullptr;
    TTF_Font* titleFont_ = nullptr;
    TTF_Font* smallFont_ = nullptr;

    void clear(SDL_Color color);
    void present();
    int drawText(TTF_Font* font, const std::string& text, int x, int y, int wrapWidth, SDL_Color color);
    void drawHeader(const std::string& title, const std::string& subtitle);
};

}  // namespace nxreader
