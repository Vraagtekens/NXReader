mod config;
mod contracts;
mod db;
mod error;
mod middleware;
mod routes;
mod services;
mod state;

use crate::{
    config::Config,
    db::connect,
    routes::router,
    services::storage::{create_s3_client, ensure_bucket},
    state::AppState,
};
use tokio::net::TcpListener;
use tower_http::{cors::CorsLayer, trace::TraceLayer};
use tracing_subscriber::{EnvFilter, layer::SubscriberExt, util::SubscriberInitExt};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    dotenvy::dotenv().ok();
    let config = Config::from_env()?;

    tracing_subscriber::registry()
        .with(
            EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "nxreader_backend=debug,tower_http=debug".into()),
        )
        .with(tracing_subscriber::fmt::layer())
        .init();

    let pool = connect(&config.database_url).await?;
    sqlx::migrate!("./migrations").run(&pool).await?;
    let s3 = create_s3_client(&config.s3).await;
    if let Err(error) = ensure_bucket(&s3, &config.s3.bucket).await {
        tracing::warn!(
            "S3 bucket '{}' is not reachable yet: {}. The API will keep running, but EPUB uploads need MinIO/S3.",
            config.s3.bucket,
            error
        );
    }

    let app = router(AppState {
        pool,
        api_key: config.api_key.clone(),
        s3,
        s3_bucket: config.s3.bucket.clone(),
    })
    .layer(CorsLayer::permissive())
    .layer(TraceLayer::new_for_http());

    let listener = TcpListener::bind(("0.0.0.0", config.port)).await?;
    tracing::info!(
        "NXReader backend listening on http://localhost:{}",
        config.port
    );
    axum::serve(listener, app).await?;

    Ok(())
}
