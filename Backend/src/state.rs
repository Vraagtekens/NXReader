use sqlx::{Pool, Postgres};

#[derive(Clone)]
pub struct AppState {
    pub pool: Pool<Postgres>,
    pub api_key: String,
    pub s3: aws_sdk_s3::Client,
    pub s3_bucket: String,
}
