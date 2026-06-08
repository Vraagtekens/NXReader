# NXReader

NXReader is a Nintendo Switch homebrew e-reader starter written in C++ with
libnx, SDL2, SDL2_ttf, and devkitPro.

This first version is intentionally small: it builds a `.nro`, browses `.epub`
files from the SD card, extracts basic EPUB text, renders it with a bundled
TrueType fonts, supports button page turns and touch word selection, toggles a dark-mode flag,
and saves the last page per book to the SD card.

## Planned App Features

- File browser for EPUB files on the SD card
- EPUB loading and chapter navigation
- Save last page per book
- Dark mode
- Touch word selection and button controls

## Requirements

Install devkitPro with the Switch toolchain:

```sh
sudo dkp-pacman -S switch-dev
sudo dkp-pacman -S switch-zlib
sudo dkp-pacman -S switch-sdl2 switch-sdl2_ttf
sudo dkp-pacman -S switch-sdl2_image
sudo dkp-pacman -S switch-curl
```

Make sure these environment variables are available in your shell:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
export DEVKITA64=$DEVKITPRO/devkitA64
```

## Build

```sh
make
```

The output will be:

```text
dist/NXReader.nro
```

To remove build artifacts:

```sh
make clean
```

If macOS creates `._*` metadata files on an external drive:

```sh
make clean-macos
```

Copy `NXReader.nro` to your Switch SD card:

```text
sdmc:/switch/NXReader/NXReader.nro
```

Then launch it from the Homebrew Menu.

## Send To Switch With Netloader

Open Netloader on the Switch, then run:

```sh
make run SWITCH_IP=192.168.1.225
```

Or call `nxlink` directly:

```sh
/opt/devkitpro/tools/bin/nxlink -a 192.168.1.225 -p /switch/NXReader.nro dist/NXReader.nro
```

In this `nxlink` version, `-p` means upload path, not port. Netloader already
uses its own port internally.

## Controls

Browser:

- D-Pad Up/Down: move selection
- `A`: open folder or book
- `B`: parent folder
- `Y`: refresh folder
- `+`: exit

Reader:

- `A`, D-Pad Right, or D-Pad Down: next page
- `B`, D-Pad Left, or D-Pad Up: previous page
- `X`: open notes/translations sheet
- `Y`: open reading settings
- `-`: back to browser
- `+`: exit
- Touch a word to select it
- With text selected, `A`: translate French to Dutch with MyMemory and save it
- With text selected, `B`: add/edit a note with the Switch keyboard
- Hold `ZL` + `ZR`, then hold D-Pad Left or Right to skim through pages

Notes/translations sheet:

- `X`: close sheet
- D-Pad Up/Down or touch-drag: move through saved items
- `A`: jump to selected item
- `-`: ask to delete selected item
- In delete dialog, `A`: delete and `B`: cancel

Chapters start on a new reader page. Page counts are generated from wrapped
reader text, so changing font size or margins will change the total page count.
The reader header appears when opening a book or pressing a reader button, then
auto-hides after a short moment. This page-turn header behavior can be disabled
in reading settings. Page slide animation can also be disabled there.

Reading settings:

- `Y`: close settings
- D-Pad Up/Down: choose setting
- D-Pad Right or `A`: increase/change selected setting
- D-Pad Left or `B`: decrease/change selected setting

Theme is changed in reading settings.

Font size is saved to `sdmc:/switch/NXReader/settings.txt`. The app bundles
Noto Sans and Noto Serif. Every `.ttf` or `.otf` font found under this folder is
added to the font selector:

```text
sdmc:/switch/NXReader/fonts/
```

It can also try `.woff2`, but `.ttf` is safest on Switch:

```text
sdmc:/switch/NXReader/fonts/open-dyslexic/opendyslexic-regular-webfont.ttf
```

A single regular font file is enough for reading. A `.ttf` is safest on Switch.
Separate bold/italic font files are nicer for real typography, but SDL_ttf can
fake bold for headings while using only the regular font.

## Books Folder

Put EPUB files here on the SD card:

```text
sdmc:/switch/NXReader/books
```

Subfolders are supported. The browser only shows folders and `.epub` files.

## Current EPUB Support

NXReader now opens the EPUB ZIP container, reads `META-INF/container.xml`,
loads the OPF package file, follows the spine order, decompresses XHTML
chapters with zlib, removes CSS/script blocks, decodes common HTML entities,
extracts cover and inline image files, strips tags, and paginates the content
into the SDL reader.

This is still a first parser pass. It does not handle CSS, embedded fonts, rich
layout, SVG images, or advanced Unicode shaping yet. PNG/JPEG covers and inline
images are rendered with SDL_image. The SDL renderer uses bundled Noto fonts, so
French accents such as `é` render as real glyphs. Page progress is saved per
book path.

## Source Layout

```text
include/nxreader/   public app headers
source/App.cpp      main event loop and mode switching
source/Browser.cpp  SD card file browser
source/Epub.cpp     EPUB ZIP/OPF/spine parser
source/Renderer.cpp SDL2/SDL_ttf renderer
source/Reader.cpp   reader state, pagination, drawing
source/Storage.cpp  save/load progress helpers
source/main.cpp     tiny app entry point
```

## Save Data

Settings are saved here:

```text
sdmc:/switch/NXReader/settings.txt
```

Page progress is saved in the same directory, keyed by EPUB path:

```text
sdmc:/switch/NXReader/progress_<book-hash>.txt
```

## Suggested Roadmap

1. Add a file browser that lists `.epub` files from `sdmc:/ebooks` and
   `sdmc:/switch/NXReader/books`.
2. Add an EPUB parser. EPUB files are ZIP archives containing metadata,
   manifests, spine order, and XHTML chapters.
3. Move from console rendering to SDL2 plus a text renderer such as SDL_ttf or
   FreeType.
4. Save progress per book.
5. Add settings for font size, margins, dark mode, and page-turn behavior.

## Notes For Rust/TypeScript Developers

The current code keeps state in a small `ReaderState` struct, similar to a
plain object in TypeScript or a simple Rust struct. Page turning is handled with
small functions that mutate that state by reference.

Memory management is deliberately simple here: no manual `new`/`delete`, no
exceptions, and no ownership-heavy abstractions yet. That keeps the starter
close to the style of many libnx examples.
