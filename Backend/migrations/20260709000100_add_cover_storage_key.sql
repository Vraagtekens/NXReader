ALTER TABLE books
    ADD COLUMN cover_storage_key text;

CREATE INDEX idx_books_cover_storage_key ON books(cover_storage_key);
