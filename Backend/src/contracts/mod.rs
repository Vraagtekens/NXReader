use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use utoipa::{
    OpenApi, ToSchema,
    openapi::security::{ApiKey, ApiKeyValue, SecurityScheme},
};
use uuid::Uuid;

#[derive(OpenApi)]
#[openapi(
    paths(
        crate::routes::health::health,
        crate::routes::books::upsert_book,
        crate::routes::books::upload_book,
        crate::routes::books::get_book,
        crate::routes::progress::get_progress,
        crate::routes::progress::put_progress,
        crate::routes::annotations::list_annotations,
        crate::routes::annotations::upsert_annotation,
        crate::routes::annotations::delete_annotation
    ),
    components(schemas(
        HealthResponse,
        Book,
        UpsertBookRequest,
        ReadingProgress,
        PutProgressRequest,
        Annotation,
        UpsertAnnotationRequest
    )),
    tags(
        (name = "health", description = "Service health"),
        (name = "books", description = "Book identity"),
        (name = "progress", description = "Cross-device reading position"),
        (name = "annotations", description = "Notes, highlights, translations")
    ),
    modifiers(&SecurityAddon),
    security(("api_key" = []))
)]
pub struct ApiDoc;

struct SecurityAddon;

impl utoipa::Modify for SecurityAddon {
    fn modify(&self, openapi: &mut utoipa::openapi::OpenApi) {
        if let Some(components) = openapi.components.as_mut() {
            components.add_security_scheme(
                "api_key",
                SecurityScheme::ApiKey(ApiKey::Header(ApiKeyValue::new("x-api-key"))),
            );
        }
    }
}

#[derive(Debug, Serialize, ToSchema)]
pub struct HealthResponse {
    pub ok: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, ToSchema, sqlx::FromRow)]
#[serde(rename_all = "camelCase")]
pub struct Book {
    pub id: Uuid,
    pub content_hash: String,
    pub title: String,
    pub author: Option<String>,
    pub file_name: Option<String>,
    pub storage_key: Option<String>,
    pub mime_type: Option<String>,
    pub file_size_bytes: Option<i64>,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
}

#[derive(Debug, Deserialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct UpsertBookRequest {
    pub content_hash: String,
    pub title: String,
    pub author: Option<String>,
    pub file_name: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, ToSchema, sqlx::FromRow)]
#[serde(rename_all = "camelCase")]
pub struct ReadingProgress {
    pub id: Uuid,
    pub book_id: Uuid,
    pub page: i32,
    pub page_count: Option<i32>,
    pub progress_percent: Option<f64>,
    pub locator: serde_json::Value,
    pub updated_at: DateTime<Utc>,
}

#[derive(Debug, Deserialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct PutProgressRequest {
    pub page: i32,
    pub page_count: Option<i32>,
    pub progress_percent: Option<f64>,
    pub locator: Option<serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize, ToSchema, sqlx::FromRow)]
#[serde(rename_all = "camelCase")]
pub struct Annotation {
    pub id: Uuid,
    pub book_id: Uuid,
    pub page: i32,
    pub selected_text: String,
    pub translation: Option<String>,
    pub note: Option<String>,
    pub selection: serde_json::Value,
    pub created_at: DateTime<Utc>,
    pub updated_at: DateTime<Utc>,
    pub deleted_at: Option<DateTime<Utc>>,
}

#[derive(Debug, Deserialize, ToSchema)]
#[serde(rename_all = "camelCase")]
pub struct UpsertAnnotationRequest {
    pub id: Option<Uuid>,
    pub page: i32,
    pub selected_text: String,
    pub translation: Option<String>,
    pub note: Option<String>,
    pub selection: Option<serde_json::Value>,
}
