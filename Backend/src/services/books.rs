use crate::contracts::{Book, UpsertBookRequest};
use sqlx::{Pool, Postgres};
use uuid::Uuid;

pub struct UploadedBook {
    pub content_hash: String,
    pub title: String,
    pub author: Option<String>,
    pub file_name: Option<String>,
    pub storage_key: String,
    pub cover_storage_key: Option<String>,
    pub mime_type: String,
    pub file_size_bytes: i64,
}

pub async fn upsert(
    pool: &Pool<Postgres>,
    request: UpsertBookRequest,
) -> Result<Book, sqlx::Error> {
    sqlx::query_as::<_, Book>(
        r#"
        INSERT INTO books (content_hash, title, author, file_name)
        VALUES ($1, $2, $3, $4)
        ON CONFLICT (content_hash)
        DO UPDATE SET
            title = EXCLUDED.title,
            author = EXCLUDED.author,
            file_name = EXCLUDED.file_name,
            updated_at = now()
        RETURNING id, content_hash, title, author, file_name, storage_key, cover_storage_key, mime_type, file_size_bytes, created_at, updated_at
        "#,
    )
    .bind(request.content_hash)
    .bind(request.title)
    .bind(request.author)
    .bind(request.file_name)
    .fetch_one(pool)
    .await
}

pub async fn upsert_uploaded(
    pool: &Pool<Postgres>,
    book: UploadedBook,
) -> Result<Book, sqlx::Error> {
    sqlx::query_as::<_, Book>(
        r#"
        INSERT INTO books (
            content_hash, title, author, file_name, storage_key, cover_storage_key, mime_type, file_size_bytes
        )
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
        ON CONFLICT (content_hash)
        DO UPDATE SET
            title = EXCLUDED.title,
            author = EXCLUDED.author,
            file_name = EXCLUDED.file_name,
            storage_key = EXCLUDED.storage_key,
            cover_storage_key = EXCLUDED.cover_storage_key,
            mime_type = EXCLUDED.mime_type,
            file_size_bytes = EXCLUDED.file_size_bytes,
            updated_at = now()
        RETURNING id, content_hash, title, author, file_name, storage_key, cover_storage_key, mime_type, file_size_bytes, created_at, updated_at
        "#,
    )
    .bind(book.content_hash)
    .bind(book.title)
    .bind(book.author)
    .bind(book.file_name)
    .bind(book.storage_key)
    .bind(book.cover_storage_key)
    .bind(book.mime_type)
    .bind(book.file_size_bytes)
    .fetch_one(pool)
    .await
}

pub async fn get(pool: &Pool<Postgres>, book_id: Uuid) -> Result<Option<Book>, sqlx::Error> {
    sqlx::query_as::<_, Book>(
        r#"
        SELECT id, content_hash, title, author, file_name, storage_key, cover_storage_key, mime_type, file_size_bytes, created_at, updated_at
        FROM books
        WHERE id = $1
        "#,
    )
    .bind(book_id)
    .fetch_optional(pool)
    .await
}

pub async fn delete(pool: &Pool<Postgres>, book_id: Uuid) -> Result<Option<Book>, sqlx::Error> {
    sqlx::query_as::<_, Book>(
        r#"
        DELETE FROM books
        WHERE id = $1
        RETURNING id, content_hash, title, author, file_name, storage_key, cover_storage_key, mime_type, file_size_bytes, created_at, updated_at
        "#,
    )
    .bind(book_id)
    .fetch_optional(pool)
    .await
}

pub async fn list(pool: &Pool<Postgres>) -> Result<Vec<Book>, sqlx::Error> {
    sqlx::query_as::<_, Book>(
        r#"
        SELECT id, content_hash, title, author, file_name, storage_key, cover_storage_key, mime_type, file_size_bytes, created_at, updated_at
        FROM books
        ORDER BY updated_at DESC
        "#,
    )
    .fetch_all(pool)
    .await
}
