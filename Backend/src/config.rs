use std::{env, error::Error};

#[derive(Debug, Clone)]
pub struct Config {
    pub port: u16,
    pub database_url: String,
    pub api_key: String,
    pub s3: S3Config,
}

#[derive(Debug, Clone)]
pub struct S3Config {
    pub endpoint: String,
    pub region: String,
    pub bucket: String,
    pub access_key_id: String,
    pub secret_access_key: String,
}

impl Config {
    pub fn from_env() -> Result<Self, Box<dyn Error>> {
        let port = env::var("PORT")
            .unwrap_or_else(|_| "3000".to_string())
            .parse::<u16>()?;

        Ok(Self {
            port,
            database_url: required_env("DATABASE_URL")?,
            api_key: required_env("API_KEY")?,
            s3: S3Config {
                endpoint: required_env("S3_ENDPOINT")?,
                region: env::var("S3_REGION").unwrap_or_else(|_| "us-east-1".to_string()),
                bucket: required_env("S3_BUCKET")?,
                access_key_id: required_env("S3_ACCESS_KEY_ID")?,
                secret_access_key: required_env("S3_SECRET_ACCESS_KEY")?,
            },
        })
    }
}

fn required_env(key: &str) -> Result<String, Box<dyn Error>> {
    env::var(key).map_err(|_| format!("{key} must be set").into())
}
