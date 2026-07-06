use nxreader_backend::contracts::ApiDoc;
use utoipa::OpenApi;

fn main() {
    let doc = ApiDoc::openapi();
    println!(
        "{}",
        doc.to_pretty_json().expect("serialize OpenAPI document")
    );
}
