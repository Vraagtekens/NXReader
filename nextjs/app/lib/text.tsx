import { ReactNode } from "react";

export function decodeEntities(value: string) {
  if (!value.includes("&")) return value;
  if (typeof document !== "undefined") {
    const textarea = document.createElement("textarea");
    textarea.innerHTML = value;
    return textarea.value;
  }
  return value
    .replace(/&ndash;/g, "–")
    .replace(/&mdash;/g, "—")
    .replace(/&rsquo;/g, "’")
    .replace(/&lsquo;/g, "‘")
    .replace(/&rdquo;/g, "”")
    .replace(/&ldquo;/g, "“")
    .replace(/&quot;/g, '"')
    .replace(/&apos;/g, "'")
    .replace(/&amp;/g, "&")
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&nbsp;/g, " ");
}

export function plain(text: string) {
  return decodeEntities(text).replace(/\*\*|_/g, "");
}

export function paginate(text: string, fontSize: number) {
  const paragraphs = decodeEntities(text).split(/\n{2,}/);
  const charactersPerPage = Math.max(900, Math.round(2300 - fontSize * 42));
  const pages: string[] = [];
  let page = "";

  for (const paragraph of paragraphs) {
    const candidate = page ? `${page}\n\n${paragraph}` : paragraph;
    if (candidate.length > charactersPerPage && page) {
      pages.push(page);
      page = paragraph;
    } else {
      page = candidate;
    }
  }
  if (page) pages.push(page);
  return pages.length ? pages : [""];
}

export function buildChapters(pages: string[]) {
  const chapters: { title: string; page: number }[] = [];
  pages.forEach((page, index) => {
    const heading = page
      .split("\n")
      .find((line) => line.startsWith("# ") || line.startsWith("## ") || line.startsWith("### "));
    if (heading) {
      chapters.push({
        title: plain(heading.replace(/^#{1,3}\s+/, "")),
        page: index + 1,
      });
    }
  });
  return chapters.length ? chapters : [{ title: "Start", page: 1 }];
}

export function withHighlights(text: string, highlights: string[]) {
  const active = highlights.filter(Boolean);
  if (!active.length) return text;
  const parts: ReactNode[] = [text];

  for (const highlight of active) {
    for (let index = 0; index < parts.length; index += 1) {
      const part = parts[index];
      if (typeof part !== "string") continue;
      const location = part.toLowerCase().indexOf(highlight.toLowerCase());
      if (location < 0) continue;
      parts.splice(
        index,
        1,
        part.slice(0, location),
        <mark key={`${highlight}-${index}`}>{part.slice(location, location + highlight.length)}</mark>,
        part.slice(location + highlight.length),
      );
    }
  }

  return parts;
}
