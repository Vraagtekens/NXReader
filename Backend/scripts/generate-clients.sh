#!/usr/bin/env sh
set -eu

OPENAPI_FILE="${1:-openapi.json}"

if ! command -v openapi-generator-cli >/dev/null 2>&1; then
  echo "openapi-generator-cli is required. Install it with npm or Homebrew first." >&2
  exit 1
fi

openapi-generator-cli generate \
  -i "$OPENAPI_FILE" \
  -g swift5 \
  -o generated/swift \
  --additional-properties=responseAs=AsyncAwait,projectName=NXReaderAPI

openapi-generator-cli generate \
  -i "$OPENAPI_FILE" \
  -g cpp-restsdk \
  -o generated/cpp \
  --additional-properties=packageName=NXReaderAPI
