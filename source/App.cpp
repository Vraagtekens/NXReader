#include "nxreader/App.hpp"

#include "nxreader/Browser.hpp"
#include "nxreader/Constants.hpp"
#include "nxreader/Epub.hpp"
#include "nxreader/Renderer.hpp"
#include "nxreader/Reader.hpp"
#include "nxreader/Storage.hpp"

#include <cstdio>
#include <cstring>
#include <switch.h>

namespace nxreader {
namespace {

enum class AppMode {
    Browser,
    Reader,
};

void openSelectedEntry(BrowserState& browser, ReaderState& reader, AppMode& mode) {
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
        loadReaderBook(reader, book, entry.name);
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

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    AppMode mode = AppMode::Browser;
    BrowserState browser = makeBrowserState();
    ReaderState reader = makeReaderState();

    scanBookDir(browser);
    renderer.drawBrowser(browser);

    bool wasTouching = false;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 buttonsDown = padGetButtonsDown(&pad);

        bool shouldRedraw = false;

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
                openSelectedEntry(browser, reader, mode);
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
            if ((buttonsDown & HidNpadButton_Minus) != 0) {
                mode = AppMode::Browser;
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_A) != 0 || (buttonsDown & HidNpadButton_Right) != 0 ||
                (buttonsDown & HidNpadButton_Down) != 0) {
                nextPage(reader);
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_B) != 0 || (buttonsDown & HidNpadButton_Left) != 0 ||
                (buttonsDown & HidNpadButton_Up) != 0) {
                previousPage(reader);
                shouldRedraw = true;
            }

            if ((buttonsDown & HidNpadButton_X) != 0) {
                reader.darkMode = !reader.darkMode;
                shouldRedraw = true;
            }

            HidTouchScreenState touchState;
            std::memset(&touchState, 0, sizeof(touchState));
            hidGetTouchScreenStates(&touchState, 1);

            const bool isTouching = touchState.count > 0;
            if (isTouching && !wasTouching) {
                const HidTouchState& touch = touchState.touches[0];
                if (touch.x < kScreenWidth / 2) {
                    previousPage(reader);
                } else {
                    nextPage(reader);
                }
                shouldRedraw = true;
            }
            wasTouching = isTouching;
        }

        if (shouldRedraw) {
            if (mode == AppMode::Browser) {
                renderer.drawBrowser(browser);
            } else {
                renderer.drawReader(reader);
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
