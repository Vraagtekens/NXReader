use crate::{
    contracts::{Annotation, UpsertAnnotationRequest},
    error::ApiResult,
    services::annotations,
    state::AppState,
};
use axum::{
    Json,
    extract::{Path, State},
};
use uuid::Uuid;

#[utoipa::path(
    get,
    path = "/books/{book_id}/annotations",
    tag = "annotations",
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, body = Vec<Annotation>))
)]
pub async fn list_annotations(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
) -> ApiResult<Json<Vec<Annotation>>> {
    Ok(Json(annotations::list(&state.pool, book_id).await?))
}

#[utoipa::path(
    put,
    path = "/books/{book_id}/annotations",
    tag = "annotations",
    request_body = UpsertAnnotationRequest,
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, body = Annotation))
)]
pub async fn upsert_annotation(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
    Json(request): Json<UpsertAnnotationRequest>,
) -> ApiResult<Json<Annotation>> {
    Ok(Json(
        annotations::upsert(&state.pool, book_id, request).await?,
    ))
}

#[utoipa::path(
    delete,
    path = "/books/{book_id}/annotations/{annotation_id}",
    tag = "annotations",
    params(
        ("book_id" = Uuid, Path, description = "Book id"),
        ("annotation_id" = Uuid, Path, description = "Annotation id")
    ),
    responses((status = 204))
)]
pub async fn delete_annotation(
    State(state): State<AppState>,
    Path((_book_id, annotation_id)): Path<(Uuid, Uuid)>,
) -> ApiResult<axum::http::StatusCode> {
    annotations::soft_delete(&state.pool, annotation_id).await?;
    Ok(axum::http::StatusCode::NO_CONTENT)
}
