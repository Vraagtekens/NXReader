use crate::config::S3Config;
use aws_config::{BehaviorVersion, Region};
use aws_credential_types::Credentials;
use aws_sdk_s3::{Client, config::Builder, primitives::ByteStream};
use bytes::Bytes;

pub async fn create_s3_client(config: &S3Config) -> Client {
    let shared_config = aws_config::defaults(BehaviorVersion::latest())
        .region(Region::new(config.region.clone()))
        .endpoint_url(config.endpoint.clone())
        .credentials_provider(Credentials::new(
            config.access_key_id.clone(),
            config.secret_access_key.clone(),
            None,
            None,
            "nxreader-minio",
        ))
        .load()
        .await;

    let s3_config = Builder::from(&shared_config).force_path_style(true).build();

    Client::from_conf(s3_config)
}

pub async fn ensure_bucket(client: &Client, bucket: &str) -> Result<(), aws_sdk_s3::Error> {
    match client.head_bucket().bucket(bucket).send().await {
        Ok(_) => Ok(()),
        Err(_) => {
            client.create_bucket().bucket(bucket).send().await?;
            Ok(())
        }
    }
}

pub async fn put_epub(
    client: &Client,
    bucket: &str,
    key: &str,
    body: Bytes,
) -> Result<(), aws_sdk_s3::Error> {
    client
        .put_object()
        .bucket(bucket)
        .key(key)
        .content_type("application/epub+zip")
        .body(ByteStream::from(body))
        .send()
        .await?;

    Ok(())
}
