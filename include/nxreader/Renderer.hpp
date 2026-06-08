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
    void playMoveSound();
    void playConfirmSound();
    void playPageTurnSound();
    void playSheetSound();
    void playDeleteSound();
    void drawBrowser(const BrowserState& state);
    void drawReader(const ReaderState& state,
                    const AppSettings& settings,
                    bool showSettings,
                    bool showAnnotationSheet,
                    bool confirmDelete,
                    int sheetOffset,
                    bool showChrome);
    void selectWordAt(ReaderState& state, bool showChrome, int x, int y, bool extendSelection);
    void drawReaderTransition(const ReaderState& state,
                              const AppSettings& settings,
                              bool showChrome,
                              int fromPage,
                              int toPage,
                              int direction,
                              int frame,
                              int totalFrames);

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_AudioDeviceID audioDevice_ = 0;
    SDL_AudioSpec audioSpec_{};
    std::vector<unsigned char> moveSound_;
    std::vector<unsigned char> confirmSound_;
    std::vector<unsigned char> pageSound_;
    std::vector<unsigned char> sheetSound_;
    std::vector<unsigned char> deleteSound_;
    std::vector<SDL_Texture*> darkButtonIcons_;
    std::vector<SDL_Texture*> lightButtonIcons_;
    SDL_Texture* transitionFromTexture_ = nullptr;
    SDL_Texture* transitionToTexture_ = nullptr;
    int transitionFromPage_ = -1;
    int transitionToPage_ = -1;
    bool transitionShowChrome_ = false;
    bool transitionDarkMode_ = false;
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
    int drawButtonHint(int icon, const std::string& label, int x, int y, SDL_Color color, bool darkMode);
    bool drawImageBytes(const std::vector<unsigned char>& bytes, int x, int y, int maxWidth, int maxHeight);
    void drawReaderChrome(const ReaderState& state, SDL_Color muted);
    void drawReaderPageContent(const ReaderState& state, int page, bool reserveChromeSpace, int xOffset, SDL_Color body);
    void drawAnnotationHighlights(const ReaderState& state, bool reserveChromeSpace);
    SDL_Texture* renderReaderPageTexture(const ReaderState& state, int page, bool showChrome, SDL_Color body);
    void clearTransitionCache();
    void playTone(float frequency, int milliseconds, float volume);
    void playSound(const std::vector<unsigned char>& sound, float fallbackFrequency, int fallbackMilliseconds, float fallbackVolume);
    std::vector<unsigned char> loadSound(const char* filename);
    SDL_Texture* loadTexture(const std::string& path);
    void loadButtonIcons();
    void destroyButtonIcons();
    SDL_Texture* buttonIcon(bool darkMode, int icon) const;
};

}  // namespace nxreader
