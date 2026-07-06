use sqlx::{Pool, Postgres, postgres::PgPoolOptions};

pub async fn connect(database_url: &str) -> Result<Pool<Postgres>, sqlx::Error> {
    PgPoolOptions::new()
        .max_connections(8)
        .connect(database_url)
        .await
}
