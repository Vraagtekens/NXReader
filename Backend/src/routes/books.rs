use crate::{
    contracts::{Book, BookRead, UpsertBookRequest},
    error::{ApiError, ApiResult},
    services::{
        books::{self, UploadedBook},
        epub, storage,
    },
    state::AppState,
};
use axum::{
    Json,
    body::Body,
    extract::{Multipart, Path, State},
    http::{StatusCode, header},
    response::Response,
};
use bytes::Bytes;
use sha2::{Digest, Sha256};
use uuid::Uuid;

#[utoipa::path(
    post,
    path = "/books",
    tag = "books",
    request_body = UpsertBookRequest,
    responses((status = 200, body = Book))
)]
pub async fn upsert_book(
    State(state): State<AppState>,
    Json(request): Json<UpsertBookRequest>,
) -> ApiResult<Json<Book>> {
    Ok(Json(books::upsert(&state.pool, request).await?))
}

#[utoipa::path(
    get,
    path = "/books",
    tag = "books",
    responses((status = 200, body = [Book]))
)]
pub async fn list_books(State(state): State<AppState>) -> ApiResult<Json<Vec<Book>>> {
    Ok(Json(books::list(&state.pool).await?))
}

#[utoipa::path(
    post,
    path = "/books/upload",
    tag = "books",
    responses((status = 200, body = Book), (status = 400))
)]
pub async fn upload_book(
    State(state): State<AppState>,
    mut multipart: Multipart,
) -> ApiResult<Json<Book>> {
    let mut title = None;
    let mut author = None;
    let mut file_name = None;
    let mut file_bytes = None;

    while let Some(field) = multipart
        .next_field()
        .await
        .map_err(|error| ApiError::BadRequest(format!("invalid multipart upload: {error}")))?
    {
        let name = field.name().unwrap_or_default().to_string();

        match name.as_str() {
            "file" => {
                file_name = field.file_name().map(ToString::to_string);
                file_bytes = Some(field.bytes().await.map_err(|error| {
                    ApiError::BadRequest(format!("could not read uploaded EPUB: {error}"))
                })?);
            }
            "title" => {
                title = Some(field.text().await.map_err(|error| {
                    ApiError::BadRequest(format!("could not read title: {error}"))
                })?);
            }
            "author" => {
                author = Some(field.text().await.map_err(|error| {
                    ApiError::BadRequest(format!("could not read author: {error}"))
                })?);
            }
            _ => {}
        }
    }

    let file_bytes =
        file_bytes.ok_or_else(|| ApiError::BadRequest("file is required".to_string()))?;
    let file_name = file_name.unwrap_or_else(|| "book.epub".to_string());

    if !file_name.to_ascii_lowercase().ends_with(".epub") {
        return Err(ApiError::BadRequest(
            "uploaded file must have a .epub extension".to_string(),
        ));
    }

    let content_hash = hash_bytes(&file_bytes);
    let storage_key = format!("{content_hash}.epub");
    storage::ensure_bucket(&state.s3, &state.s3_bucket)
        .await
        .map_err(|error| ApiError::Storage(error.to_string()))?;
    storage::put_epub(
        &state.s3,
        &state.s3_bucket,
        &storage_key,
        file_bytes.clone(),
    )
    .await
    .map_err(|error| ApiError::Storage(error.to_string()))?;

    let title = title
        .filter(|value| !value.trim().is_empty())
        .unwrap_or_else(|| title_from_file_name(&file_name));

    let book = books::upsert_uploaded(
        &state.pool,
        UploadedBook {
            content_hash,
            title,
            author: author.filter(|value| !value.trim().is_empty()),
            file_name: Some(file_name),
            storage_key,
            mime_type: "application/epub+zip".to_string(),
            file_size_bytes: file_bytes.len() as i64,
        },
    )
    .await?;

    Ok(Json(book))
}

#[utoipa::path(
    get,
    path = "/books/{book_id}",
    tag = "books",
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, body = Book), (status = 404))
)]
pub async fn get_book(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
) -> ApiResult<Json<Book>> {
    let book = books::get(&state.pool, book_id)
        .await?
        .ok_or(ApiError::NotFound)?;
    Ok(Json(book))
}

#[utoipa::path(
    get,
    path = "/books/{book_id}/download",
    tag = "books",
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, content_type = "application/epub+zip"), (status = 404))
)]
pub async fn download_book(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
) -> ApiResult<Response> {
    let book = books::get(&state.pool, book_id)
        .await?
        .ok_or(ApiError::NotFound)?;
    let storage_key = book.storage_key.clone().ok_or(ApiError::NotFound)?;
    let bytes = storage::get_epub(&state.s3, &state.s3_bucket, &storage_key)
        .await
        .map_err(|error| ApiError::Storage(error.to_string()))?;
    let file_name = book.file_name.as_deref().unwrap_or("book.epub");

    Response::builder()
        .status(StatusCode::OK)
        .header(header::CONTENT_TYPE, "application/epub+zip")
        .header(
            header::CONTENT_DISPOSITION,
            format!(
                "attachment; filename=\"{}\"",
                sanitize_header_value(file_name)
            ),
        )
        .body(Body::from(bytes))
        .map_err(|error| ApiError::Storage(error.to_string()))
}

#[utoipa::path(
    get,
    path = "/books/{book_id}/cover",
    tag = "books",
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, content_type = "image/*"), (status = 404), (status = 502))
)]
pub async fn cover_book(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
) -> ApiResult<Response> {
    let book = books::get(&state.pool, book_id)
        .await?
        .ok_or(ApiError::NotFound)?;
    let storage_key = book.storage_key.clone().ok_or(ApiError::NotFound)?;
    let bytes = storage::get_epub(&state.s3, &state.s3_bucket, &storage_key)
        .await
        .map_err(|error| ApiError::Storage(error.to_string()))?;
    let cover = epub::extract_cover(bytes)
        .map_err(ApiError::Storage)?
        .ok_or(ApiError::NotFound)?;

    Response::builder()
        .status(StatusCode::OK)
        .header(header::CONTENT_TYPE, cover.mime_type)
        .body(Body::from(cover.bytes))
        .map_err(|error| ApiError::Storage(error.to_string()))
}

#[utoipa::path(
    get,
    path = "/books/{book_id}/read",
    tag = "books",
    params(("book_id" = Uuid, Path, description = "Book id")),
    responses((status = 200, body = BookRead), (status = 404), (status = 502))
)]
pub async fn read_book(
    State(state): State<AppState>,
    Path(book_id): Path<Uuid>,
) -> ApiResult<Json<BookRead>> {
    let book = books::get(&state.pool, book_id)
        .await?
        .ok_or(ApiError::NotFound)?;
    let storage_key = book.storage_key.clone().ok_or(ApiError::NotFound)?;
    let bytes = storage::get_epub(&state.s3, &state.s3_bucket, &storage_key)
        .await
        .map_err(|error| ApiError::Storage(error.to_string()))?;
    let text = epub::extract_text(bytes).map_err(ApiError::Storage)?;

    Ok(Json(BookRead {
        book_id: book.id,
        title: book.title,
        text,
    }))
}

fn hash_bytes(bytes: &Bytes) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    hex::encode(hasher.finalize())
}

fn title_from_file_name(file_name: &str) -> String {
    file_name
        .strip_suffix(".epub")
        .or_else(|| file_name.strip_suffix(".EPUB"))
        .unwrap_or(file_name)
        .to_string()
}

fn sanitize_header_value(value: &str) -> String {
    value
        .chars()
        .filter(|character| {
            character.is_ascii_alphanumeric() || matches!(character, '.' | '-' | '_')
        })
        .collect()
}
