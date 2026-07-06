# NXReader Backend

Rust Axum REST API for syncing NXReader reading progress and annotations across Nintendo Switch and iOS clients.

## Shape

- `sqlx` + SQL migrations instead of SeaORM entities.
- `contracts` contains the Rust DTOs that become the public OpenAPI contract.
- Swift and C++ clients should be generated from `openapi.json`, so both apps share the same request/response types.

## Run locally

```sh
cp .env.example .env
sqlx database create
sqlx migrate run
cargo run
```

The OpenAPI contract is served at `http://localhost:3000/openapi.json`.

MinIO/S3 does not have to be running for the backend to start. EPUB upload requests need it, though.

All sync/book routes require your personal API key. Send it as either:

```text
x-api-key: your-api-key
```

or:

```text
Authorization: Bearer your-api-key
```

## MinIO / S3

Set the S3 values in `.env`. For a local MinIO setup, the default example uses a bucket named `books`.

```sh
S3_ENDPOINT=http://localhost:9000
S3_REGION=us-east-1
S3_BUCKET=books
S3_ACCESS_KEY_ID=minioadmin
S3_SECRET_ACCESS_KEY=minioadmin
```

Upload an EPUB with multipart form data:

```sh
curl -X POST http://localhost:3000/books/upload \
  -H "x-api-key: your-api-key" \
  -F title="Book Title" \
  -F author="Author Name" \
  -F file=@/path/to/book.epub
```

The uploaded EPUB is stored in MinIO at `<sha256>.epub`, and the matching row is upserted in `books`.

## Generate Client Contracts

```sh
cargo run --bin export-openapi > openapi.json
./scripts/generate-clients.sh
```

The generator script expects `openapi-generator-cli` on your PATH. It writes generated clients to `generated/swift` and `generated/cpp`.
