use bytes::Bytes;
use std::io::{Cursor, Read};
use zip::ZipArchive;

pub struct EpubCover {
    pub bytes: Vec<u8>,
    pub mime_type: String,
}

pub fn extract_text(bytes: Bytes) -> Result<String, String> {
    let mut archive = ZipArchive::new(Cursor::new(bytes)).map_err(|error| error.to_string())?;
    let rootfile = rootfile_path(&mut archive)?;
    let opf = read_file(&mut archive, &rootfile)?;
    let base_path = rootfile
        .rsplit_once('/')
        .map(|(base, _)| base.to_string())
        .unwrap_or_default();
    let manifest = manifest_items(&opf);
    let mut chapters = Vec::new();

    for idref in spine_ids(&opf) {
        let Some(item) = manifest.iter().find(|item| item.id == idref) else {
            continue;
        };
        if item.is_document() {
            read_chapter(
                &mut archive,
                &join_path(&base_path, &item.href),
                &mut chapters,
            );
        }
    }

    let meaningful = meaningful_chapters(&chapters);
    if !meaningful.is_empty() {
        return Ok(meaningful.join("\n\n"));
    }

    for path in html_paths(&mut archive) {
        read_chapter(&mut archive, &path, &mut chapters);
    }

    let meaningful = meaningful_chapters(&chapters);
    if !meaningful.is_empty() {
        Ok(meaningful.join("\n\n"))
    } else if let Some(text) = chapters.into_iter().find(|chapter| !chapter.is_empty()) {
        Ok(text)
    } else {
        Err("No readable XHTML chapters were found in this EPUB".to_string())
    }
}

pub fn extract_cover(bytes: Bytes) -> Result<Option<EpubCover>, String> {
    let mut archive = ZipArchive::new(Cursor::new(bytes)).map_err(|error| error.to_string())?;
    let rootfile = rootfile_path(&mut archive)?;
    let opf = read_file(&mut archive, &rootfile)?;
    let base_path = rootfile
        .rsplit_once('/')
        .map(|(base, _)| base.to_string())
        .unwrap_or_default();
    let manifest = manifest_items(&opf);
    let metadata_cover_id = metadata_cover_id(&opf);

    let cover = manifest.iter().filter(|item| item.is_image()).find(|item| {
        item.properties.contains("cover-image")
            || metadata_cover_id.as_ref() == Some(&item.id)
            || item.id.to_ascii_lowercase().contains("cover")
            || item.href.to_ascii_lowercase().contains("cover")
    });

    let Some(cover) = cover else {
        return Ok(None);
    };

    let bytes = read_binary_file(&mut archive, &join_path(&base_path, &cover.href))?;
    Ok(Some(EpubCover {
        bytes,
        mime_type: cover.inferred_mime_type(),
    }))
}

fn rootfile_path(archive: &mut ZipArchive<Cursor<Bytes>>) -> Result<String, String> {
    read_file(archive, "META-INF/container.xml")
        .ok()
        .and_then(|container| attr_value(&container, "full-path"))
        .ok_or_else(|| "EPUB container.xml does not declare a rootfile".to_string())
}

fn read_chapter(archive: &mut ZipArchive<Cursor<Bytes>>, path: &str, chapters: &mut Vec<String>) {
    let Ok(xhtml) = read_file(archive, path) else {
        return;
    };
    let text = html_to_text(&xhtml);
    if !text.is_empty() && !chapters.iter().any(|chapter| chapter == &text) {
        chapters.push(text);
    }
}

fn meaningful_chapters(chapters: &[String]) -> Vec<String> {
    chapters
        .iter()
        .filter(|chapter| is_meaningful_chapter(chapter))
        .cloned()
        .collect()
}

fn is_meaningful_chapter(text: &str) -> bool {
    let word_count = text.split_whitespace().count();
    let lowered = text.trim().to_ascii_lowercase();
    if word_count <= 3 && lowered.contains("cover") {
        return false;
    }
    word_count >= 8
}

fn html_paths(archive: &mut ZipArchive<Cursor<Bytes>>) -> Vec<String> {
    let mut paths = Vec::new();
    for index in 0..archive.len() {
        let Ok(file) = archive.by_index(index) else {
            continue;
        };
        let name = file.name().to_string();
        let lower = name.to_ascii_lowercase();
        if lower.ends_with(".xhtml") || lower.ends_with(".html") || lower.ends_with(".htm") {
            paths.push(name);
        }
    }
    paths.sort();
    paths
}

fn read_binary_file(
    archive: &mut ZipArchive<Cursor<Bytes>>,
    path: &str,
) -> Result<Vec<u8>, String> {
    let mut file = archive.by_name(path).map_err(|error| error.to_string())?;
    let mut content = Vec::new();
    file.read_to_end(&mut content)
        .map_err(|error| error.to_string())?;
    Ok(content)
}

fn read_file(archive: &mut ZipArchive<Cursor<Bytes>>, path: &str) -> Result<String, String> {
    let mut file = archive.by_name(path).map_err(|error| error.to_string())?;
    let mut content = String::new();
    file.read_to_string(&mut content)
        .map_err(|error| error.to_string())?;
    Ok(content)
}

#[derive(Clone)]
struct ManifestItem {
    id: String,
    href: String,
    media_type: String,
    properties: String,
}

impl ManifestItem {
    fn is_document(&self) -> bool {
        let href = self.href.to_ascii_lowercase();
        self.media_type.contains("xhtml")
            || self.media_type.contains("html")
            || href.ends_with(".xhtml")
            || href.ends_with(".html")
            || href.ends_with(".htm")
    }

    fn is_image(&self) -> bool {
        let href = self.href.to_ascii_lowercase();
        self.media_type.starts_with("image/")
            || href.ends_with(".jpg")
            || href.ends_with(".jpeg")
            || href.ends_with(".png")
            || href.ends_with(".webp")
            || href.ends_with(".svg")
    }

    fn inferred_mime_type(&self) -> String {
        if !self.media_type.is_empty() {
            return self.media_type.clone();
        }

        let href = self.href.to_ascii_lowercase();
        if href.ends_with(".jpg") || href.ends_with(".jpeg") {
            "image/jpeg".to_string()
        } else if href.ends_with(".png") {
            "image/png".to_string()
        } else if href.ends_with(".webp") {
            "image/webp".to_string()
        } else if href.ends_with(".svg") {
            "image/svg+xml".to_string()
        } else {
            "application/octet-stream".to_string()
        }
    }
}

fn manifest_items(opf: &str) -> Vec<ManifestItem> {
    opf.match_indices("<item")
        .filter_map(|(start, _)| {
            let end = opf[start..].find('>')? + start;
            let tag = &opf[start..=end];
            if !tag.starts_with("<item ")
                && !tag.starts_with("<item\n")
                && !tag.starts_with("<item\t")
            {
                return None;
            }
            Some(ManifestItem {
                id: attr_value(tag, "id")?,
                href: attr_value(tag, "href")?.replace("%20", " "),
                media_type: attr_value(tag, "media-type").unwrap_or_default(),
                properties: attr_value(tag, "properties").unwrap_or_default(),
            })
        })
        .collect()
}

fn metadata_cover_id(opf: &str) -> Option<String> {
    opf.match_indices("<meta").find_map(|(start, _)| {
        let end = opf[start..].find('>')? + start;
        let tag = &opf[start..=end];
        if attr_value(tag, "name").as_deref() == Some("cover") {
            attr_value(tag, "content")
        } else {
            None
        }
    })
}

fn spine_ids(opf: &str) -> Vec<String> {
    opf.match_indices("<itemref")
        .filter_map(|(start, _)| {
            let end = opf[start..].find('>')? + start;
            attr_value(&opf[start..=end], "idref")
        })
        .collect()
}

fn attr_value(tag: &str, name: &str) -> Option<String> {
    let pattern = format!("{name}=");
    let start = tag.find(&pattern)? + pattern.len();
    let quote = tag[start..].chars().next()?;
    if quote != '"' && quote != '\'' {
        return None;
    }
    let value_start = start + quote.len_utf8();
    let value_end = tag[value_start..].find(quote)? + value_start;
    Some(html_unescape(&tag[value_start..value_end]))
}

fn join_path(base: &str, href: &str) -> String {
    if base.is_empty() {
        href.to_string()
    } else {
        format!("{base}/{href}")
    }
}

fn html_to_text(html: &str) -> String {
    let mut text = String::new();
    let mut in_tag = false;
    let mut tag = String::new();
    let mut skip_depth = 0usize;

    for character in html.chars() {
        match character {
            '<' => {
                in_tag = true;
                tag.clear();
            }
            '>' if in_tag => {
                in_tag = false;
                let normalized = tag.trim().to_ascii_lowercase();
                let tag_name = tag_name(&normalized);
                let is_closing = normalized.starts_with('/');

                if normalized.starts_with("script") || normalized.starts_with("style") {
                    skip_depth += 1;
                } else if normalized.starts_with("/script") || normalized.starts_with("/style") {
                    skip_depth = skip_depth.saturating_sub(1);
                } else if skip_depth == 0 {
                    match (is_closing, tag_name.as_str()) {
                        (false, "h1") => text.push_str("\n\n# "),
                        (false, "h2") => text.push_str("\n\n## "),
                        (false, "h3") | (false, "h4") => text.push_str("\n\n### "),
                        (true, "h1" | "h2" | "h3" | "h4") => text.push_str("\n\n"),
                        (false, "strong" | "b") | (true, "strong" | "b") => text.push_str("**"),
                        (false, "em" | "i") | (true, "em" | "i") => text.push('_'),
                        (false, "li") => text.push_str("\n- "),
                        _ if is_block_tag(&normalized) => text.push('\n'),
                        _ => {}
                    }
                }
            }
            _ if in_tag => tag.push(character),
            _ if skip_depth == 0 => text.push(character),
            _ => {}
        }
    }

    normalize_whitespace(&html_unescape(&text))
}

fn tag_name(tag: &str) -> String {
    tag.trim_start_matches('/')
        .split_whitespace()
        .next()
        .unwrap_or_default()
        .trim_end_matches('/')
        .to_string()
}

fn is_block_tag(tag: &str) -> bool {
    matches!(
        tag.split_whitespace().next().unwrap_or_default(),
        "p" | "/p"
            | "br"
            | "br/"
            | "div"
            | "/div"
            | "section"
            | "/section"
            | "chapter"
            | "/chapter"
            | "h1"
            | "/h1"
            | "h2"
            | "/h2"
            | "h3"
            | "/h3"
            | "h4"
            | "/h4"
            | "li"
            | "/li"
            | "tr"
            | "/tr"
    )
}

fn normalize_whitespace(value: &str) -> String {
    let mut lines = Vec::new();
    for line in value.lines() {
        let collapsed = line.split_whitespace().collect::<Vec<_>>().join(" ");
        if !collapsed.is_empty() {
            lines.push(collapsed);
        }
    }
    lines.join("\n\n")
}

fn html_unescape(value: &str) -> String {
    value
        .replace("&nbsp;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&apos;", "'")
        .replace("&#39;", "'")
}

#[cfg(test)]
mod tests {
    use super::{extract_cover, extract_text};
    use bytes::Bytes;

    #[test]
    fn extracts_text_from_typography_sample() {
        let bytes = std::fs::read("../NXReader/samples/typography-test.epub").unwrap();
        let text = extract_text(Bytes::from(bytes)).unwrap();

        assert!(text.contains("Grand titre H1"));
        assert!(text.contains("Sous-titre H2"));
    }

    #[test]
    fn cover_extraction_is_optional() {
        let bytes = std::fs::read("../NXReader/samples/typography-test.epub").unwrap();
        let _ = extract_cover(Bytes::from(bytes)).unwrap();
    }
}
