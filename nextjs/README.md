# NXReader Web

Next.js frontend for NXReader. It provides a browser-based EPUB library and
reader that syncs with the NXReader backend.

## Routes

- `/` - home and latest book
- `/library` - synced book library, upload, remove, rename, and layout density
- `/notes` - notes and annotations
- `/reader/[bookId]` - paginated web reader

## Run

```sh
pnpm dev
```

The app reads backend configuration from public environment variables:

```sh
NEXT_PUBLIC_API_BASE_URL=http://localhost:3000
NEXT_PUBLIC_API_KEY=your-api-key
```

If those are not set, it uses the same local defaults as the iOS prototype:

```text
http://MacBookAir:3000
some-long-random-secret
```

## Build

```sh
pnpm lint
pnpm build
```
