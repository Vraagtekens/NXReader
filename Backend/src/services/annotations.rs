use crate::contracts::{Annotation, UpsertAnnotationRequest};
use sqlx::{Pool, Postgres};
use uuid::Uuid;

pub async fn list(pool: &Pool<Postgres>, book_id: Uuid) -> Result<Vec<Annotation>, sqlx::Error> {
    sqlx::query_as::<_, Annotation>(
        r#"
        SELECT id, book_id, page, selected_text, translation, note, selection,
               created_at, updated_at, deleted_at
        FROM annotations
        WHERE book_id = $1 AND deleted_at IS NULL
        ORDER BY page ASC, created_at ASC
        "#,
    )
    .bind(book_id)
    .fetch_all(pool)
    .await
}

pub async fn upsert(
    pool: &Pool<Postgres>,
    book_id: Uuid,
    request: UpsertAnnotationRequest,
) -> Result<Annotation, sqlx::Error> {
    let id = request.id.unwrap_or_else(Uuid::new_v4);

    sqlx::query_as::<_, Annotation>(
        r#"
        INSERT INTO annotations (
            id, book_id, page, selected_text, translation, note, selection
        )
        VALUES ($1, $2, $3, $4, $5, $6, COALESCE($7, '{}'::jsonb))
        ON CONFLICT (id)
        DO UPDATE SET
            page = EXCLUDED.page,
            selected_text = EXCLUDED.selected_text,
            translation = EXCLUDED.translation,
            note = EXCLUDED.note,
            selection = EXCLUDED.selection,
            updated_at = now(),
            deleted_at = NULL
        RETURNING id, book_id, page, selected_text, translation, note, selection,
                  created_at, updated_at, deleted_at
        "#,
    )
    .bind(id)
    .bind(book_id)
    .bind(request.page)
    .bind(request.selected_text)
    .bind(request.translation)
    .bind(request.note)
    .bind(request.selection)
    .fetch_one(pool)
    .await
}

pub async fn soft_delete(pool: &Pool<Postgres>, annotation_id: Uuid) -> Result<(), sqlx::Error> {
    sqlx::query(
        r#"
        UPDATE annotations
        SET deleted_at = now(), updated_at = now()
        WHERE id = $1
        "#,
    )
    .bind(annotation_id)
    .execute(pool)
    .await?;

    Ok(())
}
