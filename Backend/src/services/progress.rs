use crate::contracts::{PutProgressRequest, ReadingProgress};
use sqlx::{Pool, Postgres};
use uuid::Uuid;

pub async fn get(
    pool: &Pool<Postgres>,
    book_id: Uuid,
) -> Result<Option<ReadingProgress>, sqlx::Error> {
    sqlx::query_as::<_, ReadingProgress>(
        r#"
        SELECT id, book_id, page, page_count, progress_percent::float8, locator, updated_at
        FROM reading_progress
        WHERE book_id = $1
        "#,
    )
    .bind(book_id)
    .fetch_optional(pool)
    .await
}

pub async fn upsert(
    pool: &Pool<Postgres>,
    book_id: Uuid,
    request: PutProgressRequest,
) -> Result<ReadingProgress, sqlx::Error> {
    sqlx::query_as::<_, ReadingProgress>(
        r#"
        INSERT INTO reading_progress (
            book_id, page, page_count, progress_percent, locator
        )
        VALUES ($1, $2, $3, $4, COALESCE($5, '{}'::jsonb))
        ON CONFLICT (book_id)
        DO UPDATE SET
            page = EXCLUDED.page,
            page_count = EXCLUDED.page_count,
            progress_percent = EXCLUDED.progress_percent,
            locator = EXCLUDED.locator,
            updated_at = now()
        RETURNING id, book_id, page, page_count, progress_percent::float8, locator, updated_at
        "#,
    )
    .bind(book_id)
    .bind(request.page)
    .bind(request.page_count)
    .bind(request.progress_percent)
    .bind(request.locator)
    .fetch_one(pool)
    .await
}
