ALTER TABLE books
    ADD COLUMN storage_key text,
    ADD COLUMN mime_type text,
    ADD COLUMN file_size_bytes bigint;

CREATE INDEX idx_books_storage_key ON books(storage_key);
