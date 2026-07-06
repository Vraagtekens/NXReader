use crate::{error::ApiError, state::AppState};
use axum::{
    extract::{Request, State},
    middleware::Next,
    response::Response,
};

pub async fn require_api_key(
    State(state): State<AppState>,
    request: Request,
    next: Next,
) -> Result<Response, ApiError> {
    let provided = request
        .headers()
        .get("x-api-key")
        .and_then(|value| value.to_str().ok())
        .or_else(|| bearer_token(request.headers().get("authorization")?.to_str().ok()?));

    match provided {
        Some(value) if value == state.api_key => Ok(next.run(request).await),
        _ => Err(ApiError::Unauthorized),
    }
}

fn bearer_token(header: &str) -> Option<&str> {
    header.strip_prefix("Bearer ")
}
