ALTER TABLE reading_progress DROP CONSTRAINT IF EXISTS reading_progress_user_id_book_id_key;
ALTER TABLE books DROP CONSTRAINT IF EXISTS books_user_id_content_hash_key;

ALTER TABLE annotations DROP COLUMN IF EXISTS device_id;
ALTER TABLE annotations DROP COLUMN IF EXISTS user_id;

ALTER TABLE reading_progress DROP COLUMN IF EXISTS device_id;
ALTER TABLE reading_progress DROP COLUMN IF EXISTS user_id;

ALTER TABLE books DROP COLUMN IF EXISTS user_id;

DROP TABLE IF EXISTS devices;
DROP TABLE IF EXISTS users;

ALTER TABLE books ADD CONSTRAINT books_content_hash_key UNIQUE (content_hash);
ALTER TABLE reading_progress ADD CONSTRAINT reading_progress_book_id_key UNIQUE (book_id);
