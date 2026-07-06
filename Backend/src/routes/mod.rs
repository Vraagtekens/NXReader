pub mod annotations;
pub mod books;
pub mod health;
pub mod progress;

use crate::{contracts::ApiDoc, middleware::api_key::require_api_key, state::AppState};
use axum::{
    Json, Router, middleware,
    routing::{delete, get, post},
};
use utoipa::OpenApi;

pub fn router(state: AppState) -> Router {
    let protected = Router::new()
        .route("/books", post(books::upsert_book))
        .route("/books/upload", post(books::upload_book))
        .route("/books/{book_id}", get(books::get_book))
        .route(
            "/books/{book_id}/progress",
            get(progress::get_progress).put(progress::put_progress),
        )
        .route(
            "/books/{book_id}/annotations",
            get(annotations::list_annotations).put(annotations::upsert_annotation),
        )
        .route(
            "/books/{book_id}/annotations/{annotation_id}",
            delete(annotations::delete_annotation),
        )
        .route_layer(middleware::from_fn_with_state(
            state.clone(),
            require_api_key,
        ));

    Router::new()
        .route("/health", get(health::health))
        .route("/openapi.json", get(openapi_json))
        .merge(protected)
        .with_state(state)
}

async fn openapi_json() -> Json<utoipa::openapi::OpenApi> {
    Json(ApiDoc::openapi())
}
