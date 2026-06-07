#include "nxreader/App.hpp"

#include "nxreader/Browser.hpp"
#include "nxreader/Constants.hpp"
#include "nxreader/Epub.hpp"
#include "nxreader/Renderer.hpp"
#include "nxreader/Reader.hpp"
#include "nxreader/Settings.hpp"
#include "nxreader/Storage.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <switch.h>

namespace nxreader {
namespace {

enum class AppMode {
    Browser,
    Reader,
};

constexpr int kChromeVisibleFrames = 120;
constexpr int kRepeatDelayFrames = 28;
constexpr int kRepeatEveryFrames = 8;
constexpr int kTouchCooldownFrames = 24;

bool nextHeld(u64 buttons) {
    return (buttons & HidNpadButton_Right) != 0;
}

bool previousHeld(u64 buttons) {
    return (buttons & HidNpadButton_Left) != 0;
}

bool fastTurnModifierHeld(u64 buttons) {
    return (buttons & HidNpadButton_ZL) != 0 && (buttons & HidNpadButton_ZR) != 0;
}

void showReaderChromeAfterTurn(int& readerChromeFrames, const AppSettings& settings) {
    if (settings.showHeaderOnTurn) {
        readerChromeFrames = kChromeVisibleFrames;
    }
}

void openSelectedEntry(BrowserState& browser, ReaderState& reader, AppMode& mode, const AppSettings& settings) {
    if (browser.entries.empty()) {
        return;
    }

    const BrowserEntry& entry = browser.entries[browser.selected];
    if (entry.directory) {
        browser.currentDir = entry.path;
        browser.selected = 0;
        browser.scroll = 0;
        scanBookDir(browser);
        return;
    }

    EpubBook book;
    std::string error;
    if (loadEpub(entry.path, book, error)) {
        loadReaderBook(reader, book, entry.name, settings);
    } else {
        loadReaderError(reader, entry.name, entry.path, error);
    }
    mode = AppMode::Reader;
}

}  // namespace

int runApp() {
    fsdevMountSdmc();
    romfsInit();

    Renderer renderer;
    AppSettings settings = loadSettings();
    std::string rendererError;
    if (!renderer.init(rendererError)) {
        consoleInit(nullptr);
        std::printf("NXReader renderer error:\n\n%s\n\nPress + to exit.", rendererError.c_str());
        consoleUpdate(nullptr);
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        PadState errorPad;
        padInitializeDefault(&errorPad);
        while (appletMainLoop()) {
            padUpdate(&errorPad);
            if ((padGetButtonsDown(&errorPad) & HidNpadButton_Plus) != 0) {
                break;
            }
        }
        consoleExit(nullptr);
        romfsExit();
        fsdevUnmountDevice("sdmc");
        return 1;
    }
    renderer.applySettings(settings, rendererError);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    AppMode mode = AppMode::Browser;
    bool showSettings = false;
    int readerChromeFrames = kChromeVisibleFrames;
    int nextHoldFrames = 0;
    int previousHoldFrames = 0;
    int touchCooldownFrames = 0;
    BrowserState browser = makeBrowserState();
    ReaderState reader = makeReaderState();

    scanBookDir(browser);
    renderer.drawBrowser(browser);

    bool wasTouching = false;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 buttonsDown = padGetButtonsDown(&pad);
        const u64 buttonsHeld = padGetButtons(&pad);

        bool shouldRedraw = false;
        if (touchCooldownFrames > 0) {
            touchCooldownFrames -= 1;
        }

        if ((buttonsDown & HidNpadButton_Plus) != 0) {
            break;
        }

        if (mode == AppMode::Browser) {
            if ((buttonsDown & HidNpadButton_Down) != 0) {
                browser.selected += 1;
                clampBrowserSelection(browser);
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_Up) != 0) {
                browser.selected -= 1;
                clampBrowserSelection(browser);
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_A) != 0) {
                openSelectedEntry(browser, reader, mode, settings);
                showSettings = false;
                readerChromeFrames = kChromeVisibleFrames;
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_B) != 0) {
                enterParentDirectory(browser);
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_Y) != 0) {
                scanBookDir(browser);
                shouldRedraw = true;
            }
        } else {
            if (!showSettings && readerChromeFrames > 0) {
                readerChromeFrames -= 1;
                if (readerChromeFrames == 0) {
                    shouldRedraw = true;
                }
            }

            if ((buttonsDown & HidNpadButton_Minus) != 0) {
                mode = AppMode::Browser;
                showSettings = false;
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_Y) != 0) {
                showSettings = !showSettings;
                readerChromeFrames = kChromeVisibleFrames;
                shouldRedraw = true;
            }

            if (showSettings && ((buttonsDown & HidNpadButton_Down) != 0)) {
                settings.selectedSetting = std::min(settings.selectedSetting + 1, 2);
                shouldRedraw = true;
            } else if (showSettings && ((buttonsDown & HidNpadButton_Up) != 0)) {
                settings.selectedSetting = std::max(settings.selectedSetting - 1, 0);
                shouldRedraw = true;
            } else if (showSettings && ((buttonsDown & HidNpadButton_Right) != 0 || (buttonsDown & HidNpadButton_A) != 0)) {
                const AppSettings previousSettings = settings;
                if (settings.selectedSetting == 0) {
                    settings.fontIndex = (clampFontIndex(settings.fontIndex) + 1) % settingsFontCount();
                } else if (settings.selectedSetting == 1) {
                    settings.fontSize = clampFontSize(settings.fontSize + 2);
                } else {
                    settings.showHeaderOnTurn = !settings.showHeaderOnTurn;
                }
                if (renderer.applySettings(settings, rendererError)) {
                    repaginateReader(reader, settings);
                    saveSettings(settings);
                } else {
                    settings = previousSettings;
                    renderer.applySettings(settings, rendererError);
                }
                shouldRedraw = true;
            } else if (showSettings && ((buttonsDown & HidNpadButton_Left) != 0 || (buttonsDown & HidNpadButton_B) != 0)) {
                const AppSettings previousSettings = settings;
                if (settings.selectedSetting == 0) {
                    settings.fontIndex = (clampFontIndex(settings.fontIndex) + settingsFontCount() - 1) % settingsFontCount();
                } else if (settings.selectedSetting == 1) {
                    settings.fontSize = clampFontSize(settings.fontSize - 2);
                } else {
                    settings.showHeaderOnTurn = !settings.showHeaderOnTurn;
                }
                if (renderer.applySettings(settings, rendererError)) {
                    repaginateReader(reader, settings);
                    saveSettings(settings);
                } else {
                    settings = previousSettings;
                    renderer.applySettings(settings, rendererError);
                }
                shouldRedraw = true;
            } else if (!showSettings && ((buttonsDown & HidNpadButton_A) != 0 || (buttonsDown & HidNpadButton_Right) != 0 ||
                                         (buttonsDown & HidNpadButton_Down) != 0)) {
                nextPage(reader);
                showReaderChromeAfterTurn(readerChromeFrames, settings);
                shouldRedraw = true;
            }

            if (!showSettings && ((buttonsDown & HidNpadButton_B) != 0 || (buttonsDown & HidNpadButton_Left) != 0 ||
                                  (buttonsDown & HidNpadButton_Up) != 0)) {
                previousPage(reader);
                showReaderChromeAfterTurn(readerChromeFrames, settings);
                shouldRedraw = true;
            }

            const bool fastTurnHeld = fastTurnModifierHeld(buttonsHeld);
            if (!showSettings && fastTurnHeld && nextHeld(buttonsHeld)) {
                nextHoldFrames += 1;
                if (nextHoldFrames > kRepeatDelayFrames && nextHoldFrames % kRepeatEveryFrames == 0) {
                    nextPage(reader);
                    showReaderChromeAfterTurn(readerChromeFrames, settings);
                    shouldRedraw = true;
                }
            } else {
                nextHoldFrames = 0;
            }

            if (!showSettings && fastTurnHeld && previousHeld(buttonsHeld)) {
                previousHoldFrames += 1;
                if (previousHoldFrames > kRepeatDelayFrames && previousHoldFrames % kRepeatEveryFrames == 0) {
                    previousPage(reader);
                    showReaderChromeAfterTurn(readerChromeFrames, settings);
                    shouldRedraw = true;
                }
            } else {
                previousHoldFrames = 0;
            }

            if ((buttonsDown & HidNpadButton_X) != 0) {
                reader.darkMode = !reader.darkMode;
                readerChromeFrames = kChromeVisibleFrames;
                shouldRedraw = true;
            }

            HidTouchScreenState touchState;
            std::memset(&touchState, 0, sizeof(touchState));
            hidGetTouchScreenStates(&touchState, 1);

            const bool isTouching = touchState.count > 0;
            if (!showSettings && touchCooldownFrames == 0 && isTouching && !wasTouching) {
                const HidTouchState& touch = touchState.touches[0];
                if (touch.x < kScreenWidth / 2) {
                    previousPage(reader);
                } else {
                    nextPage(reader);
                }
                showReaderChromeAfterTurn(readerChromeFrames, settings);
                touchCooldownFrames = kTouchCooldownFrames;
                shouldRedraw = true;
            }
            wasTouching = isTouching;
        }

        if (shouldRedraw) {
            if (mode == AppMode::Browser) {
                renderer.drawBrowser(browser);
            } else {
                renderer.drawReader(reader, settings, showSettings, readerChromeFrames > 0);
            }
        }
    }

    saveLastPage(reader.bookPath.c_str(), reader.page);
    renderer.shutdown();
    romfsExit();
    fsdevUnmountDevice("sdmc");
    return 0;
}

}  // namespace nxreader
