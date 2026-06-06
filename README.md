# NXReader

NXReader is a Nintendo Switch homebrew e-reader starter written in C++ with
libnx, SDL2, SDL2_ttf, and devkitPro.

This first version is intentionally small: it builds a `.nro`, browses `.epub`
files from the SD card, extracts basic EPUB text, renders it with a bundled
TrueType font, supports button and touch page turns, toggles a dark-mode flag,
and saves the last demo page to the SD card.

## Planned App Features

- File browser for EPUB files on the SD card
- EPUB loading and chapter navigation
- Save last page per book
- Dark mode
- Touch page turning and button controls

## Requirements

Install devkitPro with the Switch toolchain:

```sh
sudo dkp-pacman -S switch-dev
sudo dkp-pacman -S switch-zlib
sudo dkp-pacman -S switch-sdl2 switch-sdl2_ttf
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
- `X`: toggle the dark-mode flag
- `-`: back to browser
- `+`: exit
- Touch the left or right side of the screen to turn pages

## Books Folder

Put EPUB files here on the SD card:

```text
sdmc:/books
```

Subfolders are supported. The browser only shows folders and `.epub` files.

## Current EPUB Support

NXReader now opens the EPUB ZIP container, reads `META-INF/container.xml`,
loads the OPF package file, follows the spine order, decompresses XHTML
chapters with zlib, removes CSS/script blocks, decodes common HTML entities,
strips tags, and paginates the plain text into the SDL reader.

This is still a first parser pass. It does not handle CSS, images, embedded
fonts, rich layout, or advanced Unicode shaping yet. The SDL renderer uses
`romfs/font.ttf`, currently Noto Sans, so French accents such as `é` render as
real glyphs. Page progress is saved per book path.

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

The demo saves the last page here:

```text
sdmc:/switch/NXReader/last_page.txt
```

Later this should become a small settings/progress file keyed by EPUB path or
book identifier.

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
