# NXReader

NXReader is a multi-platform EPUB reader project. It started as a Nintendo
Switch homebrew reader and is growing into a synced reading system with a Rust
backend, an iOS prototype, and a Next.js web app.

The goal is simple: keep a personal EPUB library available across different
frontends, with reading progress, covers, notes, highlights, and annotations
following the reader instead of being trapped on one device.

## What Is In This Repo

```text
NXReader/   Nintendo Switch homebrew app written in C++ with libnx and SDL2
Backend/    Rust Axum API for books, EPUB storage, progress, and annotations
iOS/        SwiftUI iOS prototype that connects to the backend
nextjs/     Next.js web reader for browser-based reading and library sync
```

## Frontends

### Nintendo Switch Homebrew

The original NXReader app lives in `NXReader/`. It builds a `.nro`, browses EPUB
files on the SD card, parses EPUB spine content, renders text and images with
SDL, supports controller/touch navigation, and saves local reading progress.

See [NXReader/README.md](NXReader/README.md) for devkitPro requirements,
controls, Switch build commands, and current EPUB support.

### Web App

The web app lives in `nextjs/`. It is a Next.js App Router client that mirrors
the iOS reader flow:

- Home, Library, Notes, and Reader routes
- EPUB upload through the backend
- synced book list, covers, reading progress, notes, and highlights
- local reader settings for theme, font, font size, and library density
- browser reader with paginated text and inline image support

Run it locally:

```sh
cd nextjs
pnpm dev
```

By default it expects the backend at:

```text
http://MacBookAir:3000
```

For another backend, set:

```sh
NEXT_PUBLIC_API_BASE_URL=http://localhost:3000
NEXT_PUBLIC_API_KEY=your-api-key
```

### iOS Prototype

The iOS app lives in `iOS/`. It is a SwiftUI prototype used to explore the
mobile reader experience and backend syncing model. The web app is now the main
browser-friendly direction, but the iOS code is still useful as a reference for
reader behavior and API usage.

## Backend

The backend lives in `Backend/`. It is a Rust Axum REST API backed by Postgres
and SQLx migrations, with S3-compatible object storage for EPUB files and book
covers.

Current API areas:

- book metadata and uploaded EPUB storage
- EPUB read extraction for frontend clients
- cover extraction
- cross-device reading progress
- notes, highlights, translations, and annotations
- OpenAPI export for generated clients

See [Backend/README.md](Backend/README.md) for local setup, API key usage, MinIO
/ S3 configuration, migrations, and OpenAPI client generation.

## Typical Local Setup

Start the backend first:

```sh
cd Backend
cp .env.example .env
sqlx database create
sqlx migrate run
cargo run
```

Then start the web reader:

```sh
cd nextjs
pnpm dev
```

Open the web app, upload an EPUB from the Library route, and start reading. The
web app will save progress and annotations back to the backend when the book is
stored remotely.

## Project Status

NXReader is still experimental and actively changing. The Switch app focuses on
homebrew reading, the backend focuses on sync and storage, and the web app is
becoming the most flexible frontend for everyday use.

EPUB support is practical but not complete. The parser handles core EPUB
structure, XHTML text extraction, common HTML entities, covers, and common image
types. Rich EPUB layout, CSS fidelity, embedded fonts, and advanced typography
are still future work.

## Why This Exists

Most e-readers are either locked to one ecosystem or awkward to use with a
personal EPUB collection. NXReader is a homebrew-first attempt at a reader that
is hackable, portable, and personal: bring your own books, run your own backend,
and choose the frontend that feels best.
