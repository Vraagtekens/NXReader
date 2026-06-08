#include "nxreader/App.hpp"

#include "nxreader/Browser.hpp"
#include "nxreader/Constants.hpp"
#include "nxreader/Epub.hpp"
#include "nxreader/Reader.hpp"
#include "nxreader/Renderer.hpp"
#include "nxreader/Settings.hpp"
#include "nxreader/Storage.hpp"
#include "nxreader/Translation.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <switch.h>

namespace nxreader {
namespace {

enum class AppMode {
    Browser,
    Reader,
};

constexpr int kChromeVisibleFrames = 120;
constexpr int kPageTurnFrames = 6;
constexpr int kRepeatDelayFrames = 28;
constexpr int kRepeatEveryFrames = 8;
constexpr int kAnnotationRows = 6;
constexpr int kSheetWidth = 426;
constexpr int kSheetLeft = kScreenWidth - kSheetWidth;
constexpr int kSheetAnimationFrames = 8;

struct PageTurnAnimation {
    bool active = false;
    int fromPage = 1;
    int toPage = 1;
    int direction = 1;
    int frame = 0;
};

bool nextHeld(u64 buttons) {
    return (buttons & HidNpadButton_Right) != 0;
}

bool previousHeld(u64 buttons) {
    return (buttons & HidNpadButton_Left) != 0;
}

bool fastTurnModifierHeld(u64 buttons) {
    return (buttons & HidNpadButton_ZL) != 0 && (buttons & HidNpadButton_ZR) != 0;
}

void clampAnnotationSelection(ReaderState &reader) {
    if (reader.annotations.empty()) {
        reader.selectedAnnotation = 0;
        reader.annotationScroll = 0;
        return;
    }

    reader.selectedAnnotation =
        std::max(0, std::min(reader.selectedAnnotation, static_cast<int>(reader.annotations.size()) - 1));
    if (reader.selectedAnnotation < reader.annotationScroll) {
        reader.annotationScroll = reader.selectedAnnotation;
    }
    if (reader.selectedAnnotation >= reader.annotationScroll + kAnnotationRows) {
        reader.annotationScroll = reader.selectedAnnotation - kAnnotationRows + 1;
    }
}

int findAnnotationForSelection(const ReaderState &reader) {
    for (int index = 0; index < static_cast<int>(reader.annotations.size()); ++index) {
        const Annotation &annotation = reader.annotations[index];
        if (annotation.page == reader.page && annotation.text == reader.selectedText) {
            return index;
        }
    }
    return -1;
}

Annotation &annotationForSelection(ReaderState &reader) {
    const int existingIndex = findAnnotationForSelection(reader);
    if (existingIndex >= 0) {
        return reader.annotations[existingIndex];
    }

    Annotation annotation;
    annotation.page = reader.page;
    annotation.text = reader.selectedText;
    reader.annotations.push_back(annotation);
    reader.selectedAnnotation = static_cast<int>(reader.annotations.size()) - 1;
    clampAnnotationSelection(reader);
    return reader.annotations.back();
}

bool showKeyboard(const char *header, const char *subText, const std::string &initialText, std::string &output) {
    SwkbdConfig keyboard;
    if (R_FAILED(swkbdCreate(&keyboard, 0))) {
        return false;
    }

    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, header);
    swkbdConfigSetSubText(&keyboard, subText);
    swkbdConfigSetGuideText(&keyboard, "Write text");
    swkbdConfigSetInitialText(&keyboard, initialText.c_str());
    swkbdConfigSetStringLenMax(&keyboard, 512);

    char buffer[1024] = {};
    const Result result = swkbdShow(&keyboard, buffer, sizeof(buffer));
    swkbdClose(&keyboard);
    if (R_FAILED(result)) {
        return false;
    }

    output = buffer;
    return true;
}

void editSelectionTranslation(ReaderState &reader) {
    if (reader.selectedText.empty()) {
        return;
    }

    Annotation &annotation = annotationForSelection(reader);
    std::string translation;
    std::string error;
    if (translateFrenchToDutch(annotation.text, translation, error)) {
        annotation.translation = translation;
        saveAnnotations(reader.bookPath.c_str(), reader.annotations);
        return;
    }

    std::string value;
    const std::string subText = "Auto failed: " + error;
    if (showKeyboard("Translation fr -> nl", subText.c_str(), annotation.translation, value)) {
        annotation.translation = value;
        saveAnnotations(reader.bookPath.c_str(), reader.annotations);
    }
}

void editSelectionNote(ReaderState &reader) {
    if (reader.selectedText.empty()) {
        return;
    }

    Annotation &annotation = annotationForSelection(reader);
    std::string value;
    if (showKeyboard("Note", annotation.text.c_str(), annotation.note, value)) {
        annotation.note = value;
        saveAnnotations(reader.bookPath.c_str(), reader.annotations);
    }
}

void showReaderChromeAfterTurn(int &readerChromeFrames, const AppSettings &settings) {
    if (settings.showHeaderOnTurn) {
        readerChromeFrames = kChromeVisibleFrames;
    }
}

void startPageTurnAnimation(PageTurnAnimation &animation, int fromPage, int toPage, int direction,
                            const AppSettings &settings) {
    if (fromPage == toPage || !settings.animatePageTurns) {
        animation.active = false;
        return;
    }

    animation.active = true;
    animation.fromPage = fromPage;
    animation.toPage = toPage;
    animation.direction = direction;
    animation.frame = 0;
}

bool turnNextPage(ReaderState &reader, PageTurnAnimation &animation, const AppSettings &settings) {
    const int fromPage = reader.page;
    nextPage(reader);
    startPageTurnAnimation(animation, fromPage, reader.page, 1, settings);
    return reader.page != fromPage;
}

void deleteSelectedAnnotation(ReaderState &reader) {
    if (reader.annotations.empty()) {
        return;
    }

    clampAnnotationSelection(reader);
    reader.annotations.erase(reader.annotations.begin() + reader.selectedAnnotation);
    clampAnnotationSelection(reader);
    saveAnnotations(reader.bookPath.c_str(), reader.annotations);
}

bool turnPreviousPage(ReaderState &reader, PageTurnAnimation &animation, const AppSettings &settings) {
    const int fromPage = reader.page;
    previousPage(reader);
    startPageTurnAnimation(animation, fromPage, reader.page, -1, settings);
    return reader.page != fromPage;
}

void openSelectedEntry(BrowserState &browser, ReaderState &reader, AppMode &mode, const AppSettings &settings) {
    if (browser.entries.empty()) {
        return;
    }

    const BrowserEntry &entry = browser.entries[browser.selected];
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

} // namespace

int runApp() {
    fsdevMountSdmc();
    romfsInit();
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);

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
        curl_global_cleanup();
        socketExit();
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
    bool showAnnotationSheet = false;
    bool confirmDelete = false;
    int sheetAnimationFrame = kSheetAnimationFrames;
    int readerChromeFrames = kChromeVisibleFrames;
    int nextHoldFrames = 0;
    int previousHoldFrames = 0;
    PageTurnAnimation pageTurnAnimation;
    BrowserState browser = makeBrowserState();
    ReaderState reader = makeReaderState();

    scanBookDir(browser);
    renderer.drawBrowser(browser);

    bool wasTouching = false;
    bool wasSheetTouching = false;
    int lastSheetTouchY = 0;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 buttonsDown = padGetButtonsDown(&pad);
        const u64 buttonsHeld = padGetButtons(&pad);

        bool shouldRedraw = false;
        if (pageTurnAnimation.active) {
            pageTurnAnimation.frame += 1;
            if (pageTurnAnimation.frame > kPageTurnFrames) {
                pageTurnAnimation.active = false;
            }
            shouldRedraw = true;
        }
        if ((showSettings || showAnnotationSheet) && sheetAnimationFrame < kSheetAnimationFrames) {
            sheetAnimationFrame += 1;
            shouldRedraw = true;
        }

        if ((buttonsDown & HidNpadButton_Plus) != 0) {
            break;
        }

        if (mode == AppMode::Browser) {
            if ((buttonsDown & HidNpadButton_Down) != 0) {
                browser.selected += 1;
                clampBrowserSelection(browser);
                renderer.playMoveSound();
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_Up) != 0) {
                browser.selected -= 1;
                clampBrowserSelection(browser);
                renderer.playMoveSound();
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_A) != 0) {
                openSelectedEntry(browser, reader, mode, settings);
                showSettings = false;
                showAnnotationSheet = false;
                confirmDelete = false;
                sheetAnimationFrame = kSheetAnimationFrames;
                pageTurnAnimation.active = false;
                readerChromeFrames = kChromeVisibleFrames;
                renderer.playConfirmSound();
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_B) != 0) {
                enterParentDirectory(browser);
                renderer.playMoveSound();
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_Y) != 0) {
                scanBookDir(browser);
                renderer.playConfirmSound();
                shouldRedraw = true;
            }
        } else {
            bool handledSheetAction = false;
            if (!showSettings && readerChromeFrames > 0) {
                readerChromeFrames -= 1;
                if (readerChromeFrames == 0) {
                    shouldRedraw = true;
                }
            }

            if ((buttonsDown & HidNpadButton_Minus) != 0) {
                if (confirmDelete) {
                    confirmDelete = false;
                    renderer.playMoveSound();
                } else if (showAnnotationSheet && !reader.annotations.empty()) {
                    confirmDelete = true;
                    renderer.playConfirmSound();
                } else if (showSettings || showAnnotationSheet) {
                    showSettings = false;
                    showAnnotationSheet = false;
                    renderer.playConfirmSound();
                } else {
                    mode = AppMode::Browser;
                    pageTurnAnimation.active = false;
                }
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_Y) != 0) {
                if (showSettings || showAnnotationSheet) {
                    showSettings = false;
                    showAnnotationSheet = false;
                    renderer.playConfirmSound();
                } else {
                    showSettings = false;
                    showAnnotationSheet = true;
                    sheetAnimationFrame = 0;
                    renderer.playSheetSound();
                }
                confirmDelete = false;
                pageTurnAnimation.active = false;
                reader.selectedText.clear();
                shouldRedraw = true;
            }

            if ((showSettings || showAnnotationSheet) && !confirmDelete &&
                ((buttonsDown & HidNpadButton_L) != 0 || (buttonsDown & HidNpadButton_R) != 0 ||
                 (buttonsDown & HidNpadButton_ZL) != 0 || (buttonsDown & HidNpadButton_ZR) != 0)) {
                const bool switchToSettings = showAnnotationSheet;
                showSettings = switchToSettings;
                showAnnotationSheet = !switchToSettings;
                confirmDelete = false;
                pageTurnAnimation.active = false;
                renderer.playMoveSound();
                shouldRedraw = true;
            }

            if (confirmDelete && ((buttonsDown & HidNpadButton_A) != 0)) {
                deleteSelectedAnnotation(reader);
                confirmDelete = false;
                renderer.playDeleteSound();
                shouldRedraw = true;
            } else if (confirmDelete && ((buttonsDown & HidNpadButton_B) != 0)) {
                confirmDelete = false;
                renderer.playMoveSound();
                shouldRedraw = true;
            } else if (showAnnotationSheet && ((buttonsDown & HidNpadButton_Down) != 0)) {
                reader.selectedAnnotation += 1;
                clampAnnotationSelection(reader);
                renderer.playMoveSound();
                shouldRedraw = true;
            } else if (showAnnotationSheet && ((buttonsDown & HidNpadButton_Up) != 0)) {
                reader.selectedAnnotation -= 1;
                clampAnnotationSelection(reader);
                renderer.playMoveSound();
                shouldRedraw = true;
            } else if (showAnnotationSheet && ((buttonsDown & HidNpadButton_A) != 0) && !reader.annotations.empty()) {
                clampAnnotationSelection(reader);
                reader.page = std::max(1, std::min(reader.annotations[reader.selectedAnnotation].page,
                                                   static_cast<int>(reader.pages.size())));
                reader.selectedText = reader.annotations[reader.selectedAnnotation].text;
                showAnnotationSheet = false;
                handledSheetAction = true;
                renderer.playConfirmSound();
                saveLastPage(reader.bookPath.c_str(), reader.page);
                shouldRedraw = true;
            }

            if (showSettings && ((buttonsDown & HidNpadButton_Down) != 0)) {
                settings.selectedSetting = std::min(settings.selectedSetting + 1, 4);
                renderer.playMoveSound();
                shouldRedraw = true;
            } else if (showSettings && ((buttonsDown & HidNpadButton_Up) != 0)) {
                settings.selectedSetting = std::max(settings.selectedSetting - 1, 0);
                renderer.playMoveSound();
                shouldRedraw = true;
            } else if (showSettings &&
                       ((buttonsDown & HidNpadButton_Right) != 0 || (buttonsDown & HidNpadButton_A) != 0)) {
                const AppSettings previousSettings = settings;
                if (settings.selectedSetting == 0) {
                    settings.fontIndex = (clampFontIndex(settings.fontIndex) + 1) % settingsFontCount();
                } else if (settings.selectedSetting == 1) {
                    settings.fontSize = clampFontSize(settings.fontSize + 2);
                } else if (settings.selectedSetting == 2) {
                    settings.darkMode = !settings.darkMode;
                    reader.darkMode = settings.darkMode;
                } else if (settings.selectedSetting == 3) {
                    settings.showHeaderOnTurn = !settings.showHeaderOnTurn;
                } else {
                    settings.animatePageTurns = !settings.animatePageTurns;
                }
                if (renderer.applySettings(settings, rendererError)) {
                    pageTurnAnimation.active = false;
                    repaginateReader(reader, settings);
                    saveSettings(settings);
                    renderer.playMoveSound();
                } else {
                    settings = previousSettings;
                    renderer.applySettings(settings, rendererError);
                }
                shouldRedraw = true;
            } else if (showSettings &&
                       ((buttonsDown & HidNpadButton_Left) != 0 || (buttonsDown & HidNpadButton_B) != 0)) {
                const AppSettings previousSettings = settings;
                if (settings.selectedSetting == 0) {
                    settings.fontIndex =
                        (clampFontIndex(settings.fontIndex) + settingsFontCount() - 1) % settingsFontCount();
                } else if (settings.selectedSetting == 1) {
                    settings.fontSize = clampFontSize(settings.fontSize - 2);
                } else if (settings.selectedSetting == 2) {
                    settings.darkMode = !settings.darkMode;
                    reader.darkMode = settings.darkMode;
                } else if (settings.selectedSetting == 3) {
                    settings.showHeaderOnTurn = !settings.showHeaderOnTurn;
                } else {
                    settings.animatePageTurns = !settings.animatePageTurns;
                }
                if (renderer.applySettings(settings, rendererError)) {
                    pageTurnAnimation.active = false;
                    repaginateReader(reader, settings);
                    saveSettings(settings);
                    renderer.playMoveSound();
                } else {
                    settings = previousSettings;
                    renderer.applySettings(settings, rendererError);
                }
                shouldRedraw = true;
            } else if (!handledSheetAction && !showSettings &&
                       ((buttonsDown & HidNpadButton_A) != 0 || (buttonsDown & HidNpadButton_Right) != 0 ||
                        (buttonsDown & HidNpadButton_Down) != 0)) {
                if (!showAnnotationSheet && !reader.selectedText.empty() && (buttonsDown & HidNpadButton_A) != 0) {
                    editSelectionTranslation(reader);
                    renderer.playConfirmSound();
                    shouldRedraw = true;
                } else if (!showAnnotationSheet && turnNextPage(reader, pageTurnAnimation, settings)) {
                    showReaderChromeAfterTurn(readerChromeFrames, settings);
                    renderer.playPageTurnSound();
                    shouldRedraw = true;
                }
            }

            if (!handledSheetAction && !showSettings &&
                ((buttonsDown & HidNpadButton_B) != 0 || (buttonsDown & HidNpadButton_Left) != 0 ||
                 (buttonsDown & HidNpadButton_Up) != 0)) {
                if (!showAnnotationSheet && !reader.selectedText.empty() && (buttonsDown & HidNpadButton_B) != 0) {
                    editSelectionNote(reader);
                    renderer.playConfirmSound();
                    shouldRedraw = true;
                } else if (!showAnnotationSheet && turnPreviousPage(reader, pageTurnAnimation, settings)) {
                    showReaderChromeAfterTurn(readerChromeFrames, settings);
                    renderer.playPageTurnSound();
                    shouldRedraw = true;
                }
            }

            const bool fastTurnHeld = fastTurnModifierHeld(buttonsHeld);
            if (!showSettings && !showAnnotationSheet && fastTurnHeld && nextHeld(buttonsHeld)) {
                nextHoldFrames += 1;
                if (nextHoldFrames > kRepeatDelayFrames && nextHoldFrames % kRepeatEveryFrames == 0) {
                    if (turnNextPage(reader, pageTurnAnimation, settings)) {
                        showReaderChromeAfterTurn(readerChromeFrames, settings);
                        renderer.playPageTurnSound();
                        shouldRedraw = true;
                    }
                }
            } else {
                nextHoldFrames = 0;
            }

            if (!showSettings && !showAnnotationSheet && fastTurnHeld && previousHeld(buttonsHeld)) {
                previousHoldFrames += 1;
                if (previousHoldFrames > kRepeatDelayFrames && previousHoldFrames % kRepeatEveryFrames == 0) {
                    if (turnPreviousPage(reader, pageTurnAnimation, settings)) {
                        showReaderChromeAfterTurn(readerChromeFrames, settings);
                        renderer.playPageTurnSound();
                        shouldRedraw = true;
                    }
                }
            } else {
                previousHoldFrames = 0;
            }

            HidTouchScreenState touchState;
            std::memset(&touchState, 0, sizeof(touchState));
            hidGetTouchScreenStates(&touchState, 1);

            const bool isTouching = touchState.count > 0;
            const bool sheetTouching = showAnnotationSheet && isTouching && touchState.touches[0].x >= kSheetLeft;
            if (sheetTouching && !confirmDelete) {
                const HidTouchState &touch = touchState.touches[0];
                if (!wasSheetTouching) {
                    lastSheetTouchY = touch.y;
                    const int row = (touch.y - 196) / 86;
                    if (row >= 0) {
                        reader.selectedAnnotation = reader.annotationScroll + row;
                        clampAnnotationSelection(reader);
                        renderer.playMoveSound();
                        shouldRedraw = true;
                    }
                } else {
                    const int delta = touch.y - lastSheetTouchY;
                    if (delta >= 42) {
                        reader.selectedAnnotation -= 1;
                        clampAnnotationSelection(reader);
                        lastSheetTouchY = touch.y;
                        renderer.playMoveSound();
                        shouldRedraw = true;
                    } else if (delta <= -42) {
                        reader.selectedAnnotation += 1;
                        clampAnnotationSelection(reader);
                        lastSheetTouchY = touch.y;
                        renderer.playMoveSound();
                        shouldRedraw = true;
                    }
                }
            }
            if (!showSettings && !showAnnotationSheet && isTouching && (!wasTouching || !reader.selectedText.empty())) {
                const HidTouchState &touch = touchState.touches[0];
                pageTurnAnimation.active = false;
                renderer.selectWordAt(reader, readerChromeFrames > 0, touch.x, touch.y,
                                      wasTouching && !reader.selectedText.empty());
                if (!reader.selectedText.empty() && !wasTouching) {
                    renderer.playMoveSound();
                }
                shouldRedraw = true;
            }
            wasSheetTouching = sheetTouching;
            wasTouching = isTouching;
        }

        if (shouldRedraw) {
            if (mode == AppMode::Browser) {
                renderer.drawBrowser(browser);
            } else if (pageTurnAnimation.active && !showSettings && !showAnnotationSheet) {
                renderer.drawReaderTransition(reader, settings, readerChromeFrames > 0, pageTurnAnimation.fromPage,
                                              pageTurnAnimation.toPage, pageTurnAnimation.direction,
                                              pageTurnAnimation.frame, kPageTurnFrames);
            } else {
                const bool sheetOpen = showSettings || showAnnotationSheet;
                const int clampedSheetFrame = std::max(0, std::min(sheetAnimationFrame, kSheetAnimationFrames));
                const int sheetOffset =
                    sheetOpen ? (kSheetWidth * (kSheetAnimationFrames - clampedSheetFrame)) / kSheetAnimationFrames : 0;
                renderer.drawReader(reader, settings, showSettings, showAnnotationSheet, confirmDelete, sheetOffset,
                                    readerChromeFrames > 0);
            }
        }
    }

    saveLastPage(reader.bookPath.c_str(), reader.page);
    renderer.shutdown();
    curl_global_cleanup();
    socketExit();
    romfsExit();
    fsdevUnmountDevice("sdmc");
    return 0;
}

} // namespace nxreader
