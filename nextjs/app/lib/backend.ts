import {
  BackendAnnotation,
  BackendBook,
  BackendBookRead,
  ReadingProgress,
} from "./types";

const API_BASE_URL =
  process.env.NEXT_PUBLIC_API_BASE_URL?.replace(/\/$/, "") ??
  "http://MacBookAir:3000";
const API_KEY = process.env.NEXT_PUBLIC_API_KEY ?? "some-long-random-secret";

export async function request<T = unknown>(
  path: string,
  init: RequestInit = {},
  responseType: "json" | "blob" = "json",
): Promise<T> {
  const headers = new Headers(init.headers);
  headers.set("x-api-key", API_KEY);
  const response = await fetch(`${API_BASE_URL}${path}`, { ...init, headers });
  if (!response.ok) throw new Error(`Backend returned HTTP ${response.status}.`);
  if (responseType === "blob") return (await response.blob()) as T;
  return (await response.json()) as T;
}

export function listBooks() {
  return request<BackendBook[]>("/books");
}

export function getProgress(bookId: string) {
  return request<ReadingProgress>(`/books/${bookId}/progress`);
}

export function listAnnotations(bookId: string) {
  return request<BackendAnnotation[]>(`/books/${bookId}/annotations`);
}

export function readBook(bookId: string) {
  return request<BackendBookRead>(`/books/${bookId}/read`);
}

export function coverBlob(bookId: string, signal: AbortSignal) {
  return request<Blob>(`/books/${bookId}/cover`, { signal }, "blob");
}

export function uploadBookFile(file: File) {
  const form = new FormData();
  form.append("title", file.name.replace(/\.epub$/i, ""));
  form.append("file", file);
  return request<BackendBook>("/books/upload", {
    method: "POST",
    body: form,
  });
}

export function deleteBook(bookId: string) {
  return request(`/books/${bookId}`, { method: "DELETE" });
}

export function saveProgress(bookId: string, page: number, pageCount: number) {
  const progressPercent = pageCount > 1 ? ((Math.max(1, page) - 1) / (pageCount - 1)) * 100 : 0;
  return request(`/books/${bookId}/progress`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      page,
      pageCount,
      progressPercent,
      locator: { page },
    }),
  });
}

export function saveAnnotation(
  bookId: string,
  page: number,
  selectedText: string,
  note: string | null,
) {
  return request(`/books/${bookId}/annotations`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      page,
      selectedText,
      note,
      translation: null,
      selection: { page, text: selectedText },
    }),
  });
}
