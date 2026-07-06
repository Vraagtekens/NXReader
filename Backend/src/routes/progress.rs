use crate::{
    contracts::{PutProgressRequest, ReadingProgress},
    error::{ApiError, ApiResult},
    services::progress,
    state::AppState,
};
use axum::{
    Json,
    extract::{Path, State},
};
use uuid::Uuid;

#[utoipa::path(
    get,
    path = "/books/{book_id}/progress",
    tag = "progress",
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, body = ReadingProgress), (status = 404))
)]
pub async fn get_progress(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
) -> ApiResult<Json<ReadingProgress>> {
    let progress = progress::get(&state.pool, book_id)
        .await?
        .ok_or(ApiError::NotFound)?;
    Ok(Json(progress))
}

#[utoipa::path(
    put,
    path = "/books/{book_id}/progress",
    tag = "progress",
    request_body = PutProgressRequest,
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, body = ReadingProgress))
)]
pub async fn put_progress(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
    Json(request): Json<PutProgressRequest>,
) -> ApiResult<Json<ReadingProgress>> {
    Ok(Json(progress::upsert(&state.pool, book_id, request).await?))
}
