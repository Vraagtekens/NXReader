#include "nxreader/Renderer.hpp"

#include "nxreader/Constants.hpp"
#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace nxreader {
namespace {

constexpr int kScreenHeight = 720;
constexpr int kMargin = 46;
constexpr int kReaderChromeTop = 122;
constexpr int kReaderFocusTop = 40;
constexpr int kReaderBottom = 696;
constexpr int kSheetWidth = 426;
constexpr float kPi = 3.1415926535f;
constexpr const char* kDefaultFontPath = "romfs:/font.ttf";
constexpr const char* kBundledSoundsRoot = "romfs:/sounds";
constexpr int kButtonIconSize = 28;

enum ButtonIconId {
    kIconA,
    kIconB,
    kIconY,
    kIconPlus,
    kIconMinus,
    kIconX,
    kIconL,
    kIconR,
    kIconDpadVertical,
    kIconDpadHorizontal,
    kIconCount,
};

const char* kButtonIconFiles[kIconCount]{
    "A_Button.png",
    "B_Button.png",
    "Y_Button.png",
    "Plus_Button.png",
    "Minus_Button.png",
    "X_Button.png",
    "L_Button.png",
    "R_Button.png",
    "Directional_Button_VerticalAxis.png",
    "Directional_Button_HorizontalAxis.png",
};

const char* kButtonIconFallbacks[kIconCount]{
    "A",
    "B",
    "Y",
    "+",
    "-",
    "X",
    "L",
    "R",
    "Up/Down",
    "Left/Right",
};

struct WordRect {
    std::string text;
    SDL_Rect rect{};
    int index = 0;
};

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

bool isWordByte(unsigned char value) {
    return std::isalnum(value) != 0 || value == '\'' || value == '-' || value >= 128;
}

std::string cleanSelectedWord(const std::string& value) {
    size_t start = 0;
    size_t end = value.size();
    while (start < end && !isWordByte(static_cast<unsigned char>(value[start]))) {
        start += 1;
    }
    while (end > start && !isWordByte(static_cast<unsigned char>(value[end - 1]))) {
        end -= 1;
    }
    return value.substr(start, end - start);
}

int utf8TextWidth(TTF_Font* font, const std::string& text) {
    if (text.empty()) {
        return 0;
    }

    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &width, &height) != 0) {
        return 0;
    }
    return width;
}

int wrappedTextHeight(TTF_Font* font, const std::string& text, int wrapWidth) {
    if (text.empty()) {
        return TTF_FontHeight(font);
    }

    SDL_Color white{255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), white, wrapWidth);
    if (surface == nullptr) {
        return TTF_FontHeight(font);
    }

    const int height = surface->h;
    SDL_FreeSurface(surface);
    return height;
}

std::vector<std::string> splitWords(const std::string& line) {
    std::vector<std::string> words;
    size_t wordStart = std::string::npos;
    for (size_t index = 0; index <= line.size(); ++index) {
        const bool atEnd = index == line.size();
        const unsigned char value = atEnd ? 0 : static_cast<unsigned char>(line[index]);
        const bool separator = atEnd || std::isspace(value) != 0;
        if (separator) {
            if (wordStart != std::string::npos) {
                words.push_back(line.substr(wordStart, index - wordStart));
                wordStart = std::string::npos;
            }
        } else if (wordStart == std::string::npos) {
            wordStart = index;
        }
    }
    return words;
}

bool inlineStyleMarkerAt(const std::string& text, size_t index, char& style, bool& enabled) {
    if (index + 2 >= text.size() || static_cast<unsigned char>(text[index]) != 0x1f) {
        return false;
    }

    style = text[index + 1];
    enabled = text[index + 2] == '1';
    return (style == 'I' || style == 'B') && (text[index + 2] == '0' || text[index + 2] == '1');
}

int fontStyle(bool bold, bool italic) {
    int style = TTF_STYLE_NORMAL;
    if (bold) {
        style |= TTF_STYLE_BOLD;
    }
    if (italic) {
        style |= TTF_STYLE_ITALIC;
    }
    return style;
}

TTF_Font* styledFont(TTF_Font* regularFont,
                     TTF_Font* boldFont,
                     TTF_Font* italicFont,
                     TTF_Font* boldItalicFont,
                     bool bold,
                     bool italic) {
    if (bold && italic) {
        return boldItalicFont == nullptr ? regularFont : boldItalicFont;
    }
    if (bold) {
        return boldFont == nullptr ? regularFont : boldFont;
    }
    if (italic) {
        return italicFont == nullptr ? regularFont : italicFont;
    }
    return regularFont;
}

int styledTextWidth(TTF_Font* regularFont,
                    TTF_Font* boldFont,
                    TTF_Font* italicFont,
                    TTF_Font* boldItalicFont,
                    const std::string& text,
                    bool bold,
                    bool italic) {
    if (text.empty()) {
        return 0;
    }

    TTF_Font* font = styledFont(regularFont, boldFont, italicFont, boldItalicFont, bold, italic);
    const int previousStyle = TTF_GetFontStyle(font);
    TTF_SetFontStyle(font, font == regularFont ? fontStyle(bold, italic) : TTF_STYLE_NORMAL);
    const int width = utf8TextWidth(font, text);
    TTF_SetFontStyle(font, previousStyle);
    return width;
}

void drawStyledWord(SDL_Renderer* renderer,
                    TTF_Font* regularFont,
                    TTF_Font* boldFont,
                    TTF_Font* italicFont,
                    TTF_Font* boldItalicFont,
                    const std::string& text,
                    bool bold,
                    bool italic,
                    int x,
                    int y,
                    SDL_Color color) {
    if (text.empty()) {
        return;
    }

    TTF_Font* font = styledFont(regularFont, boldFont, italicFont, boldItalicFont, bold, italic);
    const int previousStyle = TTF_GetFontStyle(font);
    TTF_SetFontStyle(font, font == regularFont ? fontStyle(bold, italic) : TTF_STYLE_NORMAL);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    TTF_SetFontStyle(font, previousStyle);
    if (surface == nullptr) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != nullptr) {
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

struct StyledWord {
    std::string text;
    bool bold = false;
    bool italic = false;
};

std::vector<StyledWord> splitStyledWords(const std::string& line) {
    std::vector<StyledWord> words;
    StyledWord current;
    bool bold = false;
    bool italic = false;

    const auto flush = [&]() {
        if (!current.text.empty()) {
            words.push_back(current);
            current.text.clear();
        }
    };

    for (size_t index = 0; index < line.size(); ++index) {
        char style = 0;
        bool enabled = false;
        if (inlineStyleMarkerAt(line, index, style, enabled)) {
            flush();
            if (style == 'I') {
                italic = enabled;
            } else {
                bold = enabled;
            }
            index += 2;
            continue;
        }

        const unsigned char value = static_cast<unsigned char>(line[index]);
        if (std::isspace(value) != 0) {
            flush();
            continue;
        }

        if (current.text.empty()) {
            current.bold = bold;
            current.italic = italic;
        }
        current.text += line[index];
    }

    flush();
    return words;
}

int drawStyledText(SDL_Renderer* renderer,
                   TTF_Font* regularFont,
                   TTF_Font* boldFont,
                   TTF_Font* italicFont,
                   TTF_Font* boldItalicFont,
                   const std::string& text,
                   int x,
                   int y,
                   int wrapWidth,
                   SDL_Color color,
                   bool draw) {
    const std::vector<StyledWord> words = splitStyledWords(text);
    const int lineHeight = TTF_FontHeight(regularFont);
    const int spaceWidth = std::max(4, utf8TextWidth(regularFont, " "));
    int cursorX = x;
    int cursorY = y;

    for (const StyledWord& word : words) {
        const int wordWidth =
            std::max(1, styledTextWidth(regularFont, boldFont, italicFont, boldItalicFont, word.text, word.bold, word.italic));
        if (cursorX > x && cursorX + wordWidth > x + wrapWidth) {
            cursorX = x;
            cursorY += lineHeight;
        }

        if (draw) {
            drawStyledWord(renderer, regularFont, boldFont, italicFont, boldItalicFont, word.text, word.bold, word.italic,
                           cursorX, cursorY, color);
        }
        cursorX += wordWidth + spaceWidth;
    }

    return words.empty() ? lineHeight : cursorY - y + lineHeight;
}

std::vector<WordRect> layoutWrappedWords(TTF_Font* font,
                                         const std::string& line,
                                         int x,
                                         int y,
                                         int wrapWidth,
                                         int firstWordIndex,
                                         int& usedHeight) {
    std::vector<WordRect> rects;
    const std::vector<std::string> words = splitWords(stripInlineStyleMarkers(line));
    const int lineHeight = TTF_FontHeight(font);
    const int spaceWidth = std::max(4, utf8TextWidth(font, " "));
    int cursorX = x;
    int cursorY = y;
    int wordIndex = firstWordIndex;

    for (const std::string& rawWord : words) {
        const std::string cleanWord = cleanSelectedWord(rawWord);
        const int wordWidth = std::max(1, utf8TextWidth(font, rawWord));
        if (cursorX > x && cursorX + wordWidth > x + wrapWidth) {
            cursorX = x;
            cursorY += lineHeight;
        }

        if (!cleanWord.empty()) {
            rects.push_back(WordRect{cleanWord, SDL_Rect{cursorX, cursorY, wordWidth, lineHeight}, wordIndex});
            wordIndex += 1;
        }
        cursorX += wordWidth + spaceWidth;
    }

    usedHeight = words.empty() ? lineHeight : cursorY - y + lineHeight;
    return rects;
}

SDL_Rect unionRects(const SDL_Rect& left, const SDL_Rect& right) {
    const int x1 = std::min(left.x, right.x);
    const int y1 = std::min(left.y, right.y);
    const int x2 = std::max(left.x + left.w, right.x + right.w);
    const int y2 = std::max(left.y + left.h, right.y + right.h);
    return SDL_Rect{x1, y1, x2 - x1, y2 - y1};
}

bool wordsMatchAt(const std::vector<WordRect>& words, int start, const std::vector<std::string>& queryWords) {
    if (start < 0 || start + static_cast<int>(queryWords.size()) > static_cast<int>(words.size())) {
        return false;
    }

    for (int offset = 0; offset < static_cast<int>(queryWords.size()); ++offset) {
        if (words[start + offset].text != queryWords[offset]) {
            return false;
        }
    }
    return true;
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

TTF_Font* openConfiguredBoldFont(int fontIndex, int fontSize) {
    TTF_Font* font = tryOpenFont(settingsFontBoldPath(fontIndex), fontSize, nullptr);
    if (font != nullptr) {
        return font;
    }
    return openConfiguredFont(fontIndex, fontSize);
}

TTF_Font* openConfiguredItalicFont(int fontIndex, int fontSize) {
    TTF_Font* font = tryOpenFont(settingsFontItalicPath(fontIndex), fontSize, nullptr);
    if (font != nullptr) {
        return font;
    }
    return openConfiguredFont(fontIndex, fontSize);
}

TTF_Font* openConfiguredBoldItalicFont(int fontIndex, int fontSize) {
    TTF_Font* font = tryOpenFont(settingsFontBoldItalicPath(fontIndex), fontSize, nullptr);
    if (font != nullptr) {
        return font;
    }
    return openConfiguredFont(fontIndex, fontSize);
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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
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

    SDL_AudioSpec desired{};
    desired.freq = 48000;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 512;
    audioDevice_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &audioSpec_, 0);
    if (audioDevice_ != 0) {
        SDL_PauseAudioDevice(audioDevice_, 0);
        moveSound_ = loadSound("SeSetNaviFocus.wav");
        confirmSound_ = loadSound("SeBtnDecide.wav");
        pageSound_ = loadSound("none.wav");
        sheetSound_ = loadSound("StartupSet.wav");
        deleteSound_ = loadSound("SeFlcGroupDelete.wav");
    }
    loadButtonIcons();
    loadUiIcons();
    return true;
}

bool Renderer::applySettings(const AppSettings& settings, std::string& error) {
    const int fontSize = clampFontSize(settings.fontSize);
    const int fontIndex = clampFontIndex(settings.fontIndex);
    if (bodyFont_ != nullptr && bodyBoldFont_ != nullptr && bodyItalicFont_ != nullptr && bodyBoldItalicFont_ != nullptr &&
        titleFont_ != nullptr && smallFont_ != nullptr && uiFont_ != nullptr && uiTitleFont_ != nullptr &&
        loadedFontSize_ == fontSize && loadedFontIndex_ == fontIndex) {
        return true;
    }

    if (bodyFont_ != nullptr) {
        TTF_CloseFont(bodyFont_);
        bodyFont_ = nullptr;
    }
    if (bodyBoldFont_ != nullptr) {
        TTF_CloseFont(bodyBoldFont_);
        bodyBoldFont_ = nullptr;
    }
    if (bodyItalicFont_ != nullptr) {
        TTF_CloseFont(bodyItalicFont_);
        bodyItalicFont_ = nullptr;
    }
    if (bodyBoldItalicFont_ != nullptr) {
        TTF_CloseFont(bodyBoldItalicFont_);
        bodyBoldItalicFont_ = nullptr;
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
    bodyBoldFont_ = openConfiguredBoldFont(fontIndex, fontSize);
    bodyItalicFont_ = openConfiguredItalicFont(fontIndex, fontSize);
    bodyBoldItalicFont_ = openConfiguredBoldItalicFont(fontIndex, fontSize);
    titleFont_ = openConfiguredBoldFont(fontIndex, fontSize + 14);
    smallFont_ = TTF_OpenFont(kDefaultFontPath, 18);
    uiFont_ = TTF_OpenFont(kDefaultFontPath, 24);
    uiTitleFont_ = TTF_OpenFont("romfs:/fonts/AtkinsonHyperlegible/AtkinsonHyperlegible-Bold.ttf", 38);
    if (bodyFont_ == nullptr || bodyBoldFont_ == nullptr || bodyItalicFont_ == nullptr || bodyBoldItalicFont_ == nullptr ||
        titleFont_ == nullptr || smallFont_ == nullptr || uiFont_ == nullptr || uiTitleFont_ == nullptr) {
        error = std::string("Could not load font: ") + fontPath + "\n" + TTF_GetError();
        return false;
    }

    loadedFontSize_ = fontSize;
    loadedFontIndex_ = fontIndex;
    loadedFontPath_ = loadedFontPath;
    clearTransitionCache();
    clearReaderBaseCache();
    return true;
}

void Renderer::shutdown() {
    clearTransitionCache();
    clearReaderBaseCache();
    destroyUiIcons();
    destroyButtonIcons();
    if (audioDevice_ != 0) {
        SDL_CloseAudioDevice(audioDevice_);
        audioDevice_ = 0;
    }
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
    if (bodyBoldItalicFont_ != nullptr) {
        TTF_CloseFont(bodyBoldItalicFont_);
        bodyBoldItalicFont_ = nullptr;
    }
    if (bodyItalicFont_ != nullptr) {
        TTF_CloseFont(bodyItalicFont_);
        bodyItalicFont_ = nullptr;
    }
    if (bodyBoldFont_ != nullptr) {
        TTF_CloseFont(bodyBoldFont_);
        bodyBoldFont_ = nullptr;
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

void Renderer::playTone(float frequency, int milliseconds, float volume) {
    if (audioDevice_ == 0) {
        return;
    }

    constexpr int sampleRate = 48000;
    const int sampleCount = std::max(1, sampleRate * milliseconds / 1000);
    std::vector<short> samples(sampleCount);
    for (int index = 0; index < sampleCount; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(sampleRate);
        const float fade = 1.0f - static_cast<float>(index) / static_cast<float>(sampleCount);
        samples[index] = static_cast<short>(std::sin(2.0f * kPi * frequency * t) * volume * fade * 32767.0f);
    }
    SDL_ClearQueuedAudio(audioDevice_);
    SDL_QueueAudio(audioDevice_, samples.data(), samples.size() * sizeof(short));
}

std::vector<unsigned char> Renderer::loadSound(const char* filename) {
    if (audioDevice_ == 0) {
        return {};
    }

    const std::vector<std::string> paths{
        std::string(kSoundsRoot) + "/" + filename,
        std::string(kSoundsRoot) + "/WAV/" + filename,
        std::string(kBundledSoundsRoot) + "/" + filename,
        std::string(kBundledSoundsRoot) + "/WAV/" + filename,
    };

    for (const std::string& path : paths) {
        SDL_AudioSpec sourceSpec{};
        Uint8* buffer = nullptr;
        Uint32 length = 0;
        if (SDL_LoadWAV(path.c_str(), &sourceSpec, &buffer, &length) == nullptr) {
            continue;
        }

        SDL_AudioCVT cvt{};
        if (SDL_BuildAudioCVT(&cvt,
                              sourceSpec.format,
                              sourceSpec.channels,
                              sourceSpec.freq,
                              audioSpec_.format,
                              audioSpec_.channels,
                              audioSpec_.freq) < 0) {
            SDL_FreeWAV(buffer);
            continue;
        }

        cvt.len = static_cast<int>(length);
        cvt.buf = static_cast<Uint8*>(SDL_malloc(length * cvt.len_mult));
        if (cvt.buf == nullptr) {
            SDL_FreeWAV(buffer);
            continue;
        }

        SDL_memcpy(cvt.buf, buffer, length);
        SDL_FreeWAV(buffer);
        if (SDL_ConvertAudio(&cvt) < 0) {
            SDL_free(cvt.buf);
            continue;
        }

        std::vector<unsigned char> sound(cvt.buf, cvt.buf + cvt.len_cvt);
        SDL_free(cvt.buf);
        return sound;
    }

    return {};
}

SDL_Texture* Renderer::loadTexture(const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (surface == nullptr) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (texture != nullptr) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

void Renderer::loadButtonIcons() {
    destroyButtonIcons();
    darkButtonIcons_.assign(kIconCount, nullptr);
    lightButtonIcons_.assign(kIconCount, nullptr);

    for (int index = 0; index < kIconCount; ++index) {
        darkButtonIcons_[index] = loadTexture(std::string("romfs:/icons/dark/") + kButtonIconFiles[index]);
        lightButtonIcons_[index] = loadTexture(std::string("romfs:/icons/light/") + kButtonIconFiles[index]);
    }
}

void Renderer::loadUiIcons() {
    destroyUiIcons();
    gridIcon_ = loadTexture("romfs:/icons/grid.png");
    listIcon_ = loadTexture("romfs:/icons/three-rows.png");
    notesIcon_ = loadTexture("romfs:/icons/clipboard-list.png");
    settingsIcon_ = loadTexture("romfs:/icons/settings.png");
}

void Renderer::destroyUiIcons() {
    SDL_Texture **textures[] = {&gridIcon_, &listIcon_, &notesIcon_, &settingsIcon_};
    for (SDL_Texture **texture : textures) {
        if (*texture != nullptr) {
            SDL_DestroyTexture(*texture);
            *texture = nullptr;
        }
    }
}

void Renderer::destroyButtonIcons() {
    for (SDL_Texture* texture : darkButtonIcons_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    for (SDL_Texture* texture : lightButtonIcons_) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    darkButtonIcons_.clear();
    lightButtonIcons_.clear();
}

SDL_Texture* Renderer::buttonIcon(bool darkMode, int icon) const {
    if (icon < 0 || icon >= kIconCount) {
        return nullptr;
    }

    const std::vector<SDL_Texture*>& icons = darkMode ? darkButtonIcons_ : lightButtonIcons_;
    if (icon >= static_cast<int>(icons.size())) {
        return nullptr;
    }
    return icons[icon];
}

void Renderer::playSound(const std::vector<unsigned char>& sound, float fallbackFrequency, int fallbackMilliseconds, float fallbackVolume) {
    if (audioDevice_ == 0 || sound.empty()) {
        playTone(fallbackFrequency, fallbackMilliseconds, fallbackVolume);
        return;
    }

    SDL_ClearQueuedAudio(audioDevice_);
    SDL_QueueAudio(audioDevice_, sound.data(), sound.size());
}

void Renderer::playMoveSound() {
    playSound(moveSound_, 740.0f, 34, 0.18f);
}

void Renderer::playConfirmSound() {
    playSound(confirmSound_, 1040.0f, 48, 0.20f);
}

void Renderer::playPageTurnSound() {
    playSound(pageSound_, 520.0f, 42, 0.16f);
}

void Renderer::playSheetSound() {
    playSound(sheetSound_, 880.0f, 60, 0.18f);
}

void Renderer::playDeleteSound() {
    playSound(deleteSound_, 330.0f, 70, 0.20f);
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

void Renderer::drawTexture(SDL_Texture* texture, int x, int y, int size) {
    if (texture == nullptr) {
        return;
    }

    SDL_Rect rect{x, y, size, size};
    SDL_RenderCopy(renderer_, texture, nullptr, &rect);
}

int Renderer::drawButtonHint(int icon, const std::string& label, int x, int y, SDL_Color color, bool darkMode) {
    SDL_Texture* texture = buttonIcon(darkMode, icon);
    int textX = x;
    if (texture != nullptr) {
        SDL_Rect rect{x, y - 3, kButtonIconSize, kButtonIconSize};
        SDL_RenderCopy(renderer_, texture, nullptr, &rect);
        textX += kButtonIconSize + 8;
    } else {
        const char* fallback = (icon >= 0 && icon < kIconCount) ? kButtonIconFallbacks[icon] : "?";
        drawSingleLine(smallFont_, fallback, x, y, 86, color);
        textX += utf8TextWidth(smallFont_, fallback) + 10;
    }

    drawSingleLine(smallFont_, label, textX, y + 2, 180, color);
    return textX + utf8TextWidth(smallFont_, label) - x + 24;
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

    drawTexture(gridIcon_, kMargin, 38, 30);
    drawSingleLine(uiTitleFont_, "NXReader", kMargin + 44, 26, 420, rgb(245, 242, 232));
    drawSingleLine(smallFont_, state.currentDir, kMargin, 76, 1060, rgb(162, 171, 181));

    if (!state.gridView) {
        int y = 126;
        const int visibleEnd = std::min(static_cast<int>(state.entries.size()), state.scroll + kVisibleRows);
        for (int index = state.scroll; index < visibleEnd; ++index) {
            const BrowserEntry& entry = state.entries[index];
            const bool selected = index == state.selected;

            if (selected) {
                SDL_Rect rect{kMargin - 14, y - 8, 1188, 46};
                SDL_SetRenderDrawColor(renderer_, 48, 62, 77, 255);
                SDL_RenderFillRect(renderer_, &rect);
                SDL_SetRenderDrawColor(renderer_, 180, 186, 198, 255);
                SDL_RenderDrawRect(renderer_, &rect);
            }

            const std::string kind = entry.directory ? "[folder] " : "";
            const std::string label = kind + (entry.title.empty() ? entry.name : entry.title);
            drawSingleLine(uiFont_, label, kMargin, y, 870, selected ? rgb(255, 255, 255) : rgb(219, 223, 228));
            if (!entry.directory) {
                drawSingleLine(smallFont_,
                               std::to_string(std::max<long long>(1, entry.size / 1024)) + " KB",
                               1060,
                               y + 7,
                               150,
                               selected ? rgb(235, 239, 245) : rgb(162, 171, 181));
            }
            y += 58;
        }

        if (!state.message.empty()) {
            drawSingleLine(smallFont_, state.message, kMargin, 626, 1060, rgb(240, 190, 120));
        }
    } else {
    const int columns = 4;
    const int tileW = 270;
    const int tileH = 238;
    const int startX = kMargin;
    const int startY = 122;
    const int gapX = 38;
    const int gapY = 34;
    const int visibleEnd = std::min(static_cast<int>(state.entries.size()), state.scroll + kVisibleRows);

    for (int index = state.scroll; index < visibleEnd; ++index) {
        const BrowserEntry& entry = state.entries[index];
        const bool selected = index == state.selected;
        const int visibleIndex = index - state.scroll;
        const int column = visibleIndex % columns;
        const int row = visibleIndex / columns;
        const int x = startX + column * (tileW + gapX);
        const int y = startY + row * (tileH + gapY);

        SDL_Rect tile{x, y, tileW, tileH};
        SDL_SetRenderDrawColor(renderer_, selected ? 58 : 31, selected ? 68 : 38, selected ? 82 : 49, 255);
        SDL_RenderFillRect(renderer_, &tile);
        SDL_SetRenderDrawColor(renderer_, selected ? 180 : 74, selected ? 186 : 82, selected ? 198 : 96, 255);
        SDL_RenderDrawRect(renderer_, &tile);

        SDL_Rect coverFrame{x + 58, y + 16, 154, 146};
        SDL_SetRenderDrawColor(renderer_, 15, 18, 22, 255);
        SDL_RenderFillRect(renderer_, &coverFrame);
        SDL_SetRenderDrawColor(renderer_, 86, 94, 108, 255);
        SDL_RenderDrawRect(renderer_, &coverFrame);

        if (entry.directory) {
            SDL_Rect folder{x + 82, y + 54, 106, 72};
            SDL_Rect tab{x + 82, y + 40, 48, 22};
            SDL_SetRenderDrawColor(renderer_, selected ? 228 : 184, selected ? 210 : 174, selected ? 142 : 112, 255);
            SDL_RenderFillRect(renderer_, &tab);
            SDL_RenderFillRect(renderer_, &folder);
            drawSingleLine(uiTitleFont_, entry.parent ? ".." : "DIR", x + 96, y + 70, 84, rgb(29, 31, 36));
        } else if (!entry.coverBytes.empty()) {
            drawImageBytes(entry.coverBytes, coverFrame.x + 8, coverFrame.y + 8, coverFrame.w - 16, coverFrame.h - 16);
        }

        const std::string label = entry.title.empty() ? entry.name : entry.title;
        drawSingleLine(smallFont_, label, x + 16, y + 174, tileW - 32, selected ? rgb(255, 255, 255) : rgb(219, 223, 228));
        if (!entry.directory) {
            drawSingleLine(smallFont_,
                           std::to_string(std::max<long long>(1, entry.size / 1024)) + " KB",
                           x + 16,
                           y + 202,
                           tileW - 32,
                           rgb(162, 171, 181));
        }
    }

    if (!state.message.empty()) {
        drawSingleLine(smallFont_, state.message, kMargin, 626, 1060, rgb(240, 190, 120));
    }
    }

    const SDL_Color footerText = rgb(162, 171, 181);
    const std::string footer = "Books " + std::to_string(state.visibleFiles) + "    Folders " +
                               std::to_string(state.visibleDirs) + "    Hidden " + std::to_string(state.hiddenFiles);
    drawSingleLine(smallFont_, footer, kMargin, 672, 430, footerText);
    int hintX = 520;
    hintX += drawButtonHint(state.gridView ? kIconDpadHorizontal : kIconDpadVertical, "Move", hintX, 670, footerText, true);
    hintX += drawButtonHint(kIconA, "Open", hintX, 670, footerText, true);
    hintX += drawButtonHint(kIconB, "Parent", hintX, 670, footerText, true);
    hintX += drawButtonHint(kIconX, state.gridView ? "List" : "Grid", hintX, 670, footerText, true);
    hintX += drawButtonHint(kIconY, "Refresh", hintX, 670, footerText, true);
    drawButtonHint(kIconPlus, "Exit", hintX, 670, footerText, true);
    present();
}

void Renderer::drawReaderChrome(const ReaderState& state, SDL_Color muted) {
    drawText(smallFont_, state.bookName, kMargin, 24, 820, muted);
    drawText(smallFont_, "Page " + std::to_string(state.page) + " / " + std::to_string(std::max<int>(1, state.pages.size())),
             kMargin, 54, 820, muted);
}

void Renderer::drawReaderPageContent(const ReaderState& state, int page, bool reserveChromeSpace, int xOffset, SDL_Color body) {
    if (state.pages.empty()) {
        return;
    }

    const int index = std::max(0, std::min(page - 1, static_cast<int>(state.pages.size()) - 1));
    if (state.pages[index] == "[[NXREADER_COVER]]") {
        const bool drewCover = drawImageBytes(state.coverImage.bytes, 170 + xOffset, 100, 940, 520);
        if (!drewCover) {
            drawText(titleFont_, "Cover image", kMargin + xOffset, 220, 900, body);
            drawText(bodyFont_, "This EPUB has a cover, but NXReader could not render its image format yet.",
                     kMargin + xOffset, 288, 1040, body);
        }
        return;
    }

    const std::vector<std::string> lines = splitLines(state.pages[index]);
    int y = reserveChromeSpace ? kReaderChromeTop : kReaderFocusTop;

    if (!state.loadError.empty()) {
        drawText(titleFont_, "EPUB load error", kMargin + xOffset, y, 880, rgb(255, 180, 120));
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
            if (y + 360 > kReaderBottom) {
                break;
            }
            const EpubImage* image = findReaderImage(state, imagePath);
            if (image != nullptr && drawImageBytes(image->bytes, kMargin + xOffset, y, 1040, 360)) {
                y += 380;
            } else {
                if (y + TTF_FontHeight(bodyFont_) > kReaderBottom) {
                    break;
                }
                drawText(bodyFont_, "[Image]", kMargin + xOffset, y, 1040, body);
                y += TTF_FontHeight(bodyFont_) + 8;
            }
            continue;
        }

        const bool heading = rawLine.rfind("## ", 0) == 0;
        const std::string line = heading ? stripInlineStyleMarkers(rawLine.substr(3)) : rawLine;
        TTF_Font* font = heading ? titleFont_ : bodyFont_;
        const int measuredHeight =
            heading ? wrappedTextHeight(font, line, 1040)
                    : drawStyledText(renderer_, bodyFont_, bodyBoldFont_, bodyItalicFont_, bodyBoldItalicFont_, line,
                                     kMargin + xOffset, y, 1040, body, false);
        if (y + measuredHeight > kReaderBottom) {
            break;
        }
        const int height =
            heading ? drawText(font, line, kMargin + xOffset, y, 1040, body)
                    : drawStyledText(renderer_, bodyFont_, bodyBoldFont_, bodyItalicFont_, bodyBoldItalicFont_, line,
                                     kMargin + xOffset, y, 1040, body, true);
        y += height + (heading ? 16 : 4);
    }
}

void Renderer::selectWordAt(ReaderState& state, bool showChrome, int x, int y, bool extendSelection) {
    if (!extendSelection) {
        state.selectedText.clear();
        state.selectedX = 0;
        state.selectedY = 0;
        state.selectedW = 0;
        state.selectedH = 0;
        state.selectionAnchor = -1;
        state.selectionFocus = -1;
        state.selectedRects.clear();
    }

    if (state.pages.empty() || x < kMargin || x > kScreenWidth - kMargin || y > kReaderBottom) {
        return;
    }

    const int index = std::max(0, std::min(state.page - 1, static_cast<int>(state.pages.size()) - 1));
    if (state.pages[index] == "[[NXREADER_COVER]]") {
        return;
    }

    const std::vector<std::string> lines = splitLines(state.pages[index]);
    int cursorY = showChrome ? kReaderChromeTop : kReaderFocusTop;
    int wordIndex = 0;
    std::vector<WordRect> pageWords;
    if (!state.loadError.empty()) {
        cursorY += 58;
    }

    for (const std::string& rawLine : lines) {
        if (cursorY > kReaderBottom) {
            break;
        }

        if (rawLine.empty()) {
            cursorY += 8;
            continue;
        }

        std::string imagePath;
        if (imageMarkerPath(rawLine, imagePath)) {
            cursorY += 380;
            continue;
        }

        const bool heading = rawLine.rfind("## ", 0) == 0;
        const std::string line = heading ? stripInlineStyleMarkers(rawLine.substr(3)) : rawLine;
        TTF_Font* font = heading ? titleFont_ : bodyFont_;
        int height = 0;
        std::vector<WordRect> lineWords = layoutWrappedWords(font, line, kMargin, cursorY, 1040, wordIndex, height);
        wordIndex += static_cast<int>(lineWords.size());
        pageWords.insert(pageWords.end(), lineWords.begin(), lineWords.end());
        cursorY += height + (heading ? 16 : 4);
    }

    int touchedIndex = -1;
    for (const WordRect& word : pageWords) {
        if (x >= word.rect.x && x <= word.rect.x + word.rect.w && y >= word.rect.y && y <= word.rect.y + word.rect.h) {
            touchedIndex = word.index;
            break;
        }
    }

    if (touchedIndex < 0) {
        return;
    }

    if (!extendSelection || state.selectionAnchor < 0) {
        state.selectionAnchor = touchedIndex;
    }
    state.selectionFocus = touchedIndex;

    const int first = std::min(state.selectionAnchor, state.selectionFocus);
    const int last = std::max(state.selectionAnchor, state.selectionFocus);
    SDL_Rect selectionRect{};
    bool haveRect = false;
    state.selectedText.clear();
    state.selectedRects.clear();

    for (const WordRect& word : pageWords) {
        if (word.index < first || word.index > last) {
            continue;
        }

        if (!state.selectedText.empty()) {
            state.selectedText += " ";
        }
        state.selectedText += word.text;
        state.selectedRects.push_back(SelectionRect{word.rect.x, word.rect.y, word.rect.w, word.rect.h});

        selectionRect = haveRect ? unionRects(selectionRect, word.rect) : word.rect;
        haveRect = true;
    }

    if (haveRect) {
        state.selectedX = selectionRect.x;
        state.selectedY = selectionRect.y;
        state.selectedW = selectionRect.w;
        state.selectedH = selectionRect.h;
    }
}

void Renderer::drawAnnotationHighlights(const ReaderState& state, bool reserveChromeSpace) {
    if (state.pages.empty() || state.annotations.empty()) {
        return;
    }

    const int pageIndex = std::max(0, std::min(state.page - 1, static_cast<int>(state.pages.size()) - 1));
    if (state.pages[pageIndex] == "[[NXREADER_COVER]]") {
        return;
    }

    std::vector<WordRect> pageWords;
    int cursorY = reserveChromeSpace ? kReaderChromeTop : kReaderFocusTop;
    int wordIndex = 0;
    if (!state.loadError.empty()) {
        cursorY += 58;
    }

    for (const std::string& rawLine : splitLines(state.pages[pageIndex])) {
        if (cursorY > kReaderBottom) {
            break;
        }
        if (rawLine.empty()) {
            cursorY += 8;
            continue;
        }

        std::string imagePath;
        if (imageMarkerPath(rawLine, imagePath)) {
            cursorY += 380;
            continue;
        }

        const bool heading = rawLine.rfind("## ", 0) == 0;
        const std::string line = heading ? stripInlineStyleMarkers(rawLine.substr(3)) : rawLine;
        TTF_Font* font = heading ? titleFont_ : bodyFont_;
        int height = 0;
        std::vector<WordRect> lineWords = layoutWrappedWords(font, line, kMargin, cursorY, 1040, wordIndex, height);
        wordIndex += static_cast<int>(lineWords.size());
        pageWords.insert(pageWords.end(), lineWords.begin(), lineWords.end());
        cursorY += height + (heading ? 16 : 4);
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, state.darkMode ? 130 : 250, state.darkMode ? 110 : 218, state.darkMode ? 42 : 120, 62);
    for (const Annotation& annotation : state.annotations) {
        if (annotation.page != state.page || annotation.text.empty()) {
            continue;
        }

        const std::vector<std::string> queryWords = splitWords(annotation.text);
        if (queryWords.empty()) {
            continue;
        }

        for (int index = 0; index < static_cast<int>(pageWords.size()); ++index) {
            if (!wordsMatchAt(pageWords, index, queryWords)) {
                continue;
            }

            for (int offset = 0; offset < static_cast<int>(queryWords.size()); ++offset) {
                const SDL_Rect& rect = pageWords[index + offset].rect;
                SDL_Rect highlight{rect.x - 3, rect.y - 2, rect.w + 6, rect.h + 4};
                SDL_RenderFillRect(renderer_, &highlight);
            }
            break;
        }
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

void Renderer::clearTransitionCache() {
    if (transitionFromTexture_ != nullptr) {
        SDL_DestroyTexture(transitionFromTexture_);
        transitionFromTexture_ = nullptr;
    }
    if (transitionToTexture_ != nullptr) {
        SDL_DestroyTexture(transitionToTexture_);
        transitionToTexture_ = nullptr;
    }
    transitionFromPage_ = -1;
    transitionToPage_ = -1;
    transitionShowChrome_ = false;
    transitionDarkMode_ = false;
}

void Renderer::clearReaderBaseCache() {
    if (readerBaseTexture_ != nullptr) {
        SDL_DestroyTexture(readerBaseTexture_);
        readerBaseTexture_ = nullptr;
    }
    readerBasePage_ = -1;
    readerBaseDarkMode_ = false;
}

SDL_Texture* Renderer::renderReaderPageTexture(const ReaderState& state, int page, bool showChrome, SDL_Color body) {
    SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, kScreenWidth, kScreenHeight);
    if (texture == nullptr) {
        return nullptr;
    }

    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (SDL_SetRenderTarget(renderer_, texture) != 0) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
    drawReaderPageContent(state, page, showChrome, 0, body);
    drawAnnotationHighlights(state, showChrome);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer_, previousTarget);
    return texture;
}

SDL_Texture* Renderer::renderReaderBaseTexture(const ReaderState& state, bool showChrome, SDL_Color body) {
    SDL_Texture* texture =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, kScreenWidth, kScreenHeight);
    if (texture == nullptr) {
        return nullptr;
    }

    SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer_);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    if (SDL_SetRenderTarget(renderer_, texture) != 0) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    clear(state.darkMode ? rgb(14, 17, 21) : rgb(236, 232, 220));
    drawReaderPageContent(state, state.page, showChrome, 0, body);
    drawAnnotationHighlights(state, showChrome);
    SDL_SetRenderTarget(renderer_, previousTarget);
    return texture;
}

void Renderer::drawReader(const ReaderState& state,
                          const AppSettings& settings,
                          bool showSettings,
                          bool showAnnotationSheet,
                          bool confirmDelete,
                          int sheetOffset,
    bool showChrome) {
    clearTransitionCache();

    const SDL_Color body = state.darkMode ? rgb(238, 235, 225) : rgb(25, 28, 33);
    const SDL_Color muted = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);
    const bool sheetOpen = showSettings || showAnnotationSheet;

    if (sheetOpen) {
        const bool cacheMatches =
            readerBaseTexture_ != nullptr && readerBasePage_ == state.page && readerBaseDarkMode_ == state.darkMode;
        if (!cacheMatches) {
            clearReaderBaseCache();
            readerBaseTexture_ = renderReaderBaseTexture(state, showChrome, body);
            readerBasePage_ = state.page;
            readerBaseDarkMode_ = state.darkMode;
        }

        if (readerBaseTexture_ != nullptr) {
            SDL_RenderCopy(renderer_, readerBaseTexture_, nullptr, nullptr);
        } else {
            clear(state.darkMode ? rgb(14, 17, 21) : rgb(236, 232, 220));
            drawReaderPageContent(state, state.page, showChrome, 0, body);
            drawAnnotationHighlights(state, showChrome);
        }
    } else {
        clearReaderBaseCache();
        clear(state.darkMode ? rgb(14, 17, 21) : rgb(236, 232, 220));
        if (showChrome) {
            drawReaderChrome(state, muted);
        }
        drawReaderPageContent(state, state.page, showChrome, 0, body);
        drawAnnotationHighlights(state, showChrome);
    }

    if (settings.showPageCounter && !sheetOpen) {
        const std::string counter =
            std::to_string(state.page) + " / " + std::to_string(std::max<int>(1, state.pages.size()));
        const int width = utf8TextWidth(smallFont_, counter);
        SDL_Rect panel{kScreenWidth - width - 90, 654, width + 44, 36};
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 24 : 245, state.darkMode ? 28 : 242, state.darkMode ? 34 : 234, 228);
        SDL_RenderFillRect(renderer_, &panel);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        drawSingleLine(smallFont_, counter, panel.x + (panel.w - width) / 2, panel.y + (panel.h - TTF_FontHeight(smallFont_)) / 2,
                       width + 4, muted);
    }

    if (!state.selectedText.empty() && !showSettings && !showAnnotationSheet) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 96 : 74, state.darkMode ? 128 : 139, state.darkMode ? 180 : 214, 96);
        for (const SelectionRect& selectedRect : state.selectedRects) {
            SDL_Rect highlight{selectedRect.x - 3, selectedRect.y - 2, selectedRect.w + 6, selectedRect.h + 4};
            SDL_RenderFillRect(renderer_, &highlight);
        }

        const int panelWidth = 800;
        const int panelHeight = 58;
        const int panelX = std::max(28, std::min(kScreenWidth - panelWidth - 28, state.selectedX + state.selectedW / 2 - panelWidth / 2));
        const int belowY = state.selectedY + state.selectedH + 14;
        const int aboveY = state.selectedY - panelHeight - 14;
        const int panelY = belowY + panelHeight <= kReaderBottom ? belowY : std::max(96, aboveY);
        SDL_Rect panel{panelX, panelY, panelWidth, panelHeight};
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 36 : 248, state.darkMode ? 42 : 244, state.darkMode ? 52 : 236, 248);
        SDL_RenderFillRect(renderer_, &panel);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 88 : 176, state.darkMode ? 96 : 170, state.darkMode ? 108 : 158, 255);
        SDL_RenderDrawRect(renderer_, &panel);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        const SDL_Color panelText = state.darkMode ? rgb(245, 242, 232) : rgb(25, 28, 33);
        const SDL_Color mutedText = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);
        drawSingleLine(uiFont_, state.selectedText, panel.x + 30, panel.y + 15, 330, panelText);
        int hintX = panel.x + 390;
        hintX += drawButtonHint(kIconA, "Translate", hintX, panel.y + 18, mutedText, state.darkMode);
        drawButtonHint(kIconB, "Note", hintX, panel.y + 18, mutedText, state.darkMode);
    }

    if (sheetOpen) {
        const int panelX = kScreenWidth - kSheetWidth + std::max(0, sheetOffset);
        SDL_Rect panel{panelX, 0, kSheetWidth, kScreenHeight};
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 25 : 246, state.darkMode ? 30 : 247, state.darkMode ? 38 : 249, 255);
        SDL_RenderFillRect(renderer_, &panel);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 74 : 196, state.darkMode ? 82 : 200, state.darkMode ? 94 : 206, 255);
        SDL_RenderDrawRect(renderer_, &panel);

        const SDL_Color panelText = state.darkMode ? rgb(245, 242, 232) : rgb(25, 28, 33);
        const SDL_Color mutedText = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);
        const SDL_Color selectedText = state.darkMode ? rgb(255, 255, 255) : rgb(12, 20, 30);
        const SDL_Color activeTab = state.darkMode ? rgb(238, 241, 245) : rgb(24, 29, 36);
        const SDL_Color inactiveTab = state.darkMode ? rgb(138, 146, 156) : rgb(95, 101, 110);
        const int contentX = panelX + 30;
        const int contentWidth = kSheetWidth - 60;

        drawSingleLine(uiTitleFont_, "Library", contentX, 28, contentWidth, panelText);
        SDL_Rect notesTab{contentX, 88, 160, 46};
        SDL_Rect settingsTab{contentX + 172, 88, 160, 46};
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 47 : 230, state.darkMode ? 54 : 234, state.darkMode ? 66 : 240, 255);
        SDL_RenderFillRect(renderer_, showAnnotationSheet ? &notesTab : &settingsTab);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 90 : 178, state.darkMode ? 98 : 184, state.darkMode ? 112 : 194, 255);
        SDL_RenderDrawRect(renderer_, &notesTab);
        SDL_RenderDrawRect(renderer_, &settingsTab);
        drawTexture(notesIcon_, notesTab.x + 16, notesTab.y + 12, 22);
        drawTexture(settingsIcon_, settingsTab.x + 14, settingsTab.y + 12, 22);
        drawSingleLine(uiFont_, "Notes", notesTab.x + 46, notesTab.y + 10, notesTab.w - 58, showAnnotationSheet ? activeTab : inactiveTab);
        drawSingleLine(uiFont_, "Settings", settingsTab.x + 42, settingsTab.y + 10, settingsTab.w - 52, showSettings ? activeTab : inactiveTab);

        if (showSettings) {
            const SDL_Color settingSelected = state.darkMode ? rgb(255, 255, 255) : rgb(12, 20, 30);
            const std::string fontName = settingsFontName(settings.fontIndex);
            const std::string rows[5]{
                "Font: " + fontName,
                "Size: " + std::to_string(settings.fontSize),
                std::string("Theme: ") + (settings.darkMode ? "Dark" : "Light"),
                std::string("Page counter: ") + (settings.showPageCounter ? "On" : "Off"),
                std::string("Page animation: ") + (settings.animatePageTurns ? "On" : "Off"),
            };
            int y = 164;
            for (int index = 0; index < 5; ++index) {
                const bool selected = settings.selectedSetting == index;
                if (selected) {
                    SDL_Rect row{contentX - 12, y - 8, contentWidth + 24, 48};
                    SDL_SetRenderDrawColor(renderer_, state.darkMode ? 56 : 226, state.darkMode ? 64 : 232, state.darkMode ? 78 : 240, 255);
                    SDL_RenderFillRect(renderer_, &row);
                    SDL_SetRenderDrawColor(renderer_, state.darkMode ? 150 : 112, state.darkMode ? 156 : 122, state.darkMode ? 168 : 136, 255);
                    SDL_RenderDrawRect(renderer_, &row);
                }
                drawSingleLine(uiFont_, rows[index], contentX, y, contentWidth, selected ? settingSelected : panelText);
                y += 58;
            }
            int hintX = contentX;
            hintX += drawButtonHint(kIconL, "Tabs", hintX, 556, mutedText, state.darkMode);
            hintX += drawButtonHint(kIconR, "Tabs", hintX, 556, mutedText, state.darkMode);
            drawButtonHint(kIconDpadVertical, "Select", contentX, 584, mutedText, state.darkMode);
            drawButtonHint(kIconDpadHorizontal, "Change", contentX + 172, 584, mutedText, state.darkMode);
            drawButtonHint(kIconY, "Close", contentX, 612, mutedText, state.darkMode);
            const std::string loadedFont = loadedFontPath_.empty() ? "unknown" : filenameFromPath(loadedFontPath_);
            drawSingleLine(smallFont_, "Loaded: " + loadedFont, contentX, 652, contentWidth, mutedText);
        } else {
            int hintX = contentX;
            hintX += drawButtonHint(kIconA, "Open", hintX, 146, mutedText, state.darkMode);
            hintX += drawButtonHint(kIconMinus, "Delete", hintX, 146, mutedText, state.darkMode);
            drawButtonHint(kIconL, "Settings", hintX, 146, mutedText, state.darkMode);
            if (state.annotations.empty()) {
                drawText(smallFont_, "No saved translations or notes for this book yet.", contentX, 206, contentWidth, mutedText);
            }

            int y = 196;
            const int end = std::min(static_cast<int>(state.annotations.size()), state.annotationScroll + 6);
            for (int index = state.annotationScroll; index < end; ++index) {
                const Annotation& annotation = state.annotations[index];
                const bool selected = index == state.selectedAnnotation;
                if (selected) {
                    SDL_Rect row{contentX - 12, y - 8, contentWidth + 24, 78};
                    SDL_SetRenderDrawColor(renderer_, state.darkMode ? 56 : 226, state.darkMode ? 64 : 232, state.darkMode ? 78 : 240, 255);
                    SDL_RenderFillRect(renderer_, &row);
                    SDL_SetRenderDrawColor(renderer_, state.darkMode ? 150 : 112, state.darkMode ? 156 : 122, state.darkMode ? 168 : 136, 255);
                    SDL_RenderDrawRect(renderer_, &row);
                }

                drawSingleLine(smallFont_, "p." + std::to_string(annotation.page) + "  " + annotation.text,
                               contentX, y, contentWidth, selected ? selectedText : panelText);
                const std::string detail = !annotation.translation.empty() ? annotation.translation : annotation.note;
                drawText(smallFont_, detail.empty() ? "(empty)" : detail, contentX, y + 26, contentWidth, selected ? selectedText : mutedText);
                y += 86;
            }
        }
    }

    if (confirmDelete) {
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 120);
        SDL_Rect shade{0, 0, kScreenWidth, kScreenHeight};
        SDL_RenderFillRect(renderer_, &shade);

        SDL_Rect dialog{340, 216, 600, 248};
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 36 : 248, state.darkMode ? 42 : 246, state.darkMode ? 52 : 240, 255);
        SDL_RenderFillRect(renderer_, &dialog);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 102 : 178, state.darkMode ? 112 : 174, state.darkMode ? 126 : 164, 255);
        SDL_RenderDrawRect(renderer_, &dialog);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);

        const SDL_Color dialogText = state.darkMode ? rgb(245, 242, 232) : rgb(25, 28, 33);
        const SDL_Color mutedText = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);
        drawSingleLine(uiTitleFont_, "Delete item?", 386, 252, 510, dialogText);
        drawText(smallFont_, "This removes the saved translation/note for this book.", 386, 314, 510, mutedText);
        drawButtonHint(kIconA, "Delete", 386, 392, rgb(212, 78, 78), state.darkMode);
        drawButtonHint(kIconB, "Cancel", 620, 392, dialogText, state.darkMode);
    }
    present();
}

void Renderer::drawReaderTransition(const ReaderState& state,
                                    const AppSettings& settings,
                                    bool showChrome,
                                    int fromPage,
                                    int toPage,
                                    int direction,
                                    int frame,
                                    int totalFrames) {
    clear(state.darkMode ? rgb(14, 17, 21) : rgb(236, 232, 220));

    const SDL_Color body = state.darkMode ? rgb(238, 235, 225) : rgb(25, 28, 33);
    const SDL_Color muted = state.darkMode ? rgb(164, 172, 181) : rgb(82, 88, 96);
    const int clampedTotal = std::max(1, totalFrames);
    const int clampedFrame = std::max(0, std::min(frame, clampedTotal));
    const int distance = (kScreenWidth * clampedFrame) / clampedTotal;
    const int oldOffset = direction < 0 ? distance : -distance;
    const int newOffset = direction < 0 ? distance - kScreenWidth : kScreenWidth - distance;
    const bool cacheMatches = transitionFromTexture_ != nullptr && transitionToTexture_ != nullptr &&
                              transitionFromPage_ == fromPage && transitionToPage_ == toPage &&
                              transitionShowChrome_ == showChrome && transitionDarkMode_ == state.darkMode;
    if (!cacheMatches) {
        clearTransitionCache();
        transitionFromTexture_ = renderReaderPageTexture(state, fromPage, showChrome, body);
        transitionToTexture_ = renderReaderPageTexture(state, toPage, showChrome, body);
        transitionFromPage_ = fromPage;
        transitionToPage_ = toPage;
        transitionShowChrome_ = showChrome;
        transitionDarkMode_ = state.darkMode;
    }

    SDL_Rect contentClip{0, showChrome ? 96 : 0, kScreenWidth, kScreenHeight - (showChrome ? 96 : 0)};
    SDL_RenderSetClipRect(renderer_, &contentClip);
    if (transitionFromTexture_ != nullptr && transitionToTexture_ != nullptr) {
        SDL_Rect oldRect{oldOffset, 0, kScreenWidth, kScreenHeight};
        SDL_Rect newRect{newOffset, 0, kScreenWidth, kScreenHeight};
        SDL_RenderCopy(renderer_, transitionFromTexture_, nullptr, &oldRect);
        SDL_RenderCopy(renderer_, transitionToTexture_, nullptr, &newRect);
    } else {
        drawReaderPageContent(state, fromPage, showChrome, oldOffset, body);
        drawReaderPageContent(state, toPage, showChrome, newOffset, body);
    }
    SDL_RenderSetClipRect(renderer_, nullptr);

    if (showChrome) {
        drawReaderChrome(state, muted);
    }
    if (settings.showPageCounter) {
        const std::string counter =
            std::to_string(state.page) + " / " + std::to_string(std::max<int>(1, state.pages.size()));
        const int width = utf8TextWidth(smallFont_, counter);
        SDL_Rect panel{kScreenWidth - width - 90, 654, width + 44, 36};
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer_, state.darkMode ? 24 : 245, state.darkMode ? 28 : 242,
                               state.darkMode ? 34 : 234, 228);
        SDL_RenderFillRect(renderer_, &panel);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        drawSingleLine(smallFont_, counter, panel.x + (panel.w - width) / 2, panel.y + (panel.h - TTF_FontHeight(smallFont_)) / 2,
                       width + 4, muted);
    }
    (void)settings;
    present();
}

}  // namespace nxreader
