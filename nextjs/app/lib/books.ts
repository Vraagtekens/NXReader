import { BackendAnnotation, BackendBook, ReaderBook, ReaderNote } from "./types";
import { sampleText } from "./sample-library";
import { decodeEntities } from "./text";

export function mapBackendBook(book: BackendBook): ReaderBook {
  return {
    id: book.id,
    title: book.title,
    author: book.author?.trim() || "Unknown Author",
    fileName: book.fileName,
    storageKey: book.storageKey,
    coverColorHex: coverColorFor(book.contentHash),
    currentPage: 1,
    pageCount: 1,
    lastOpenedAt: book.updatedAt,
    sampleText,
    images: [],
    highlights: [],
  };
}

export function coverColorFor(value: string) {
  const colors = ["3454D1", "1F8A70", "C44536", "8A4FFF", "2F4858"];
  const sum = [...value].reduce((total, character) => total + character.charCodeAt(0), 0);
  return colors[sum % colors.length];
}

export function progressPercent(book: ReaderBook) {
  if (book.pageCount <= 1) return 0;
  return Math.round(((clamp(book.currentPage, 1, book.pageCount) - 1) / (book.pageCount - 1)) * 100);
}

export function progressLabel(book: ReaderBook) {
  return `${progressPercent(book)}%`;
}

export function clamp(value: number, min: number, max: number) {
  return Math.min(Math.max(value, min), Math.max(min, max));
}

export function mergeNotes(
  current: ReaderNote[],
  books: BackendBook[],
  annotationsById: Record<string, BackendAnnotation[]>,
) {
  const remoteNotes = books.flatMap((book) =>
    (annotationsById[book.id] ?? [])
      .filter((annotation) => annotation.note?.trim())
      .map((annotation) => ({
        id: annotation.id,
        bookTitle: book.title,
        page: annotation.page,
        selectedText: decodeEntities(annotation.selectedText),
        note: decodeEntities(annotation.note ?? ""),
        createdAt: annotation.createdAt,
      })),
  );
  const byId = new Map<string, ReaderNote>();
  [...remoteNotes, ...current].forEach((note) => byId.set(note.id, note));
  return [...byId.values()].sort(
    (a, b) => Date.parse(b.createdAt) - Date.parse(a.createdAt),
  );
}

export function messageFromError(error: unknown) {
  return error instanceof Error ? error.message : "Something went wrong.";
}
