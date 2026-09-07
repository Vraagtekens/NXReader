"use client";

import {
  ChangeEvent,
  ReactNode,
  createContext,
  useContext,
  useEffect,
  useMemo,
  useState,
} from "react";
import {
  deleteBook,
  getProgress,
  listAnnotations,
  listBooks,
  readBook,
  saveAnnotation,
  saveProgress,
  uploadBookFile,
} from "../lib/backend";
import { clamp, coverColorFor, mapBackendBook, mergeNotes, messageFromError } from "../lib/books";
import { sampleBooks, sampleNotes, sampleText } from "../lib/sample-library";
import {
  BackendAnnotation,
  ReaderBook,
  ReaderNote,
  ReaderSettings,
} from "../lib/types";

const STORAGE_KEY = "nxreader.web.state";

type LibraryContextValue = {
  books: ReaderBook[];
  notes: ReaderNote[];
  settings: ReaderSettings;
  syncError: string | null;
  isUploading: boolean;
  isRefreshing: boolean;
  setSettings: (settings: ReaderSettings) => void;
  refreshBooks: () => Promise<void>;
  uploadBook: (event: ChangeEvent<HTMLInputElement>) => Promise<void>;
  loadBook: (bookId: string) => Promise<void>;
  removeBook: (book: ReaderBook) => Promise<void>;
  renameBook: (book: ReaderBook) => void;
  markFinished: (book: ReaderBook) => void;
  markOpened: (bookId: string) => void;
  updateProgress: (bookId: string, page: number, pageCount: number) => void;
  addHighlight: (bookId: string, selectedText: string) => void;
  addNote: (book: ReaderBook, page: number, selectedText: string, note: string) => void;
};

const LibraryContext = createContext<LibraryContextValue | null>(null);

export function LibraryProvider({ children }: { children: ReactNode }) {
  const savedState = useMemo(readSavedState, []);
  const [books, setBooks] = useState<ReaderBook[]>(() =>
    sampleBooks.map((book) => ({
      ...book,
      highlights: savedState.highlights?.[book.id] ?? book.highlights,
    })),
  );
  const [notes, setNotes] = useState<ReaderNote[]>(savedState.notes ?? sampleNotes);
  const [syncError, setSyncError] = useState<string | null>(null);
  const [isUploading, setIsUploading] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [settings, setSettings] = useState<ReaderSettings>(
    savedState.settings ?? {
      fontSize: 22,
      fontFamily: "georgia",
      theme: "dark",
      libraryColumns: 3,
    },
  );

  useEffect(() => {
    const highlights = Object.fromEntries(
      books.map((book) => [book.id, book.highlights]),
    );
    window.localStorage.setItem(
      STORAGE_KEY,
      JSON.stringify({ notes, highlights, settings }),
    );
  }, [books, notes, settings]);

  useEffect(() => {
    void refreshBooks();
  }, []);

  async function refreshBooks() {
    setIsRefreshing(true);
    try {
      const remoteBooks = await listBooks();
      const progressEntries = await Promise.all(
        remoteBooks.map(async (book) => {
          try {
            return [book.id, await getProgress(book.id)] as const;
          } catch {
            return [book.id, null] as const;
          }
        }),
      );
      const annotationEntries = await Promise.all(
        remoteBooks.map(async (book) => {
          try {
            return [book.id, await listAnnotations(book.id)] as const;
          } catch {
            return [book.id, []] as const;
          }
        }),
      );
      const progressById = Object.fromEntries(progressEntries);
      const annotationsById = Object.fromEntries(annotationEntries) as Record<
        string,
        BackendAnnotation[]
      >;

      setBooks((current) => {
        const existing = Object.fromEntries(current.map((book) => [book.id, book]));
        return remoteBooks.map((book) => {
          const progress = progressById[book.id];
          const annotations = annotationsById[book.id] ?? [];
          const previous = existing[book.id];
          return {
            id: book.id,
            title: book.title,
            author: book.author?.trim() || "Unknown Author",
            fileName: book.fileName,
            storageKey: book.storageKey,
            coverColorHex: coverColorFor(book.contentHash),
            currentPage: Math.max(1, progress?.page ?? previous?.currentPage ?? 1),
            pageCount: Math.max(1, progress?.pageCount ?? previous?.pageCount ?? 1),
            lastOpenedAt: book.updatedAt,
            sampleText: previous?.sampleText ?? sampleText,
            images: previous?.images ?? [],
            highlights: [
              ...new Set([
                ...(previous?.highlights ?? []),
                ...annotations
                  .filter((annotation) => !annotation.note?.trim())
                  .map((annotation) => annotation.selectedText),
              ]),
            ],
          };
        });
      });
      setNotes((current) => mergeNotes(current, remoteBooks, annotationsById));
      setSyncError(null);
    } catch (error) {
      setSyncError(messageFromError(error));
    } finally {
      setIsRefreshing(false);
    }
  }

  async function uploadBook(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;

    setIsUploading(true);
    try {
      const uploaded = await uploadBookFile(file);
      const book = mapBackendBook(uploaded);
      setBooks((current) => [book, ...current.filter((item) => item.id !== book.id)]);
      setSyncError(null);
    } catch (error) {
      setSyncError(messageFromError(error));
    } finally {
      setIsUploading(false);
    }
  }

  async function loadBook(bookId: string) {
    const book = books.find((item) => item.id === bookId);
    markOpened(bookId);
    if (!book?.storageKey || book.sampleText !== sampleText) return;

    try {
      const [read, progress] = await Promise.all([
        readBook(book.id),
        getProgress(book.id).catch(() => null),
      ]);
      setBooks((current) =>
        current.map((item) =>
          item.id === book.id
            ? {
                ...item,
                sampleText: read.text || sampleText,
                images: read.images ?? [],
                currentPage: Math.max(1, progress?.page ?? item.currentPage),
                pageCount: Math.max(1, progress?.pageCount ?? item.pageCount),
              }
            : item,
        ),
      );
      setSyncError(null);
    } catch (error) {
      setSyncError(messageFromError(error));
    }
  }

  async function removeBook(book: ReaderBook) {
    if (book.storageKey) {
      try {
        await deleteBook(book.id);
      } catch (error) {
        setSyncError(messageFromError(error));
        return;
      }
    }
    setBooks((current) => current.filter((item) => item.id !== book.id));
  }

  function renameBook(book: ReaderBook) {
    const title = window.prompt("Rename Book", book.title)?.trim();
    if (!title) return;
    setBooks((current) =>
      current.map((item) => (item.id === book.id ? { ...item, title } : item)),
    );
  }

  function markFinished(book: ReaderBook) {
    const pageCount = Math.max(1, book.pageCount);
    updateProgress(book.id, pageCount, pageCount);
  }

  function markOpened(bookId: string) {
    setBooks((current) =>
      current
        .map((book) =>
          book.id === bookId ? { ...book, lastOpenedAt: new Date().toISOString() } : book,
        )
        .sort((a, b) => Date.parse(b.lastOpenedAt) - Date.parse(a.lastOpenedAt)),
    );
  }

  function updateProgress(bookId: string, page: number, pageCount: number) {
    setBooks((current) =>
      current.map((book) =>
        book.id === bookId
          ? { ...book, currentPage: clamp(page, 1, pageCount), pageCount }
          : book,
      ),
    );
    const book = books.find((item) => item.id === bookId);
    if (!book?.storageKey) return;
    void saveProgress(bookId, page, pageCount).catch((error) =>
      setSyncError(messageFromError(error)),
    );
  }

  function addHighlight(bookId: string, selectedText: string) {
    const text = selectedText.trim();
    if (!text) return;
    const book = books.find((item) => item.id === bookId);
    setBooks((current) =>
      current.map((book) =>
        book.id === bookId && !book.highlights.includes(text)
          ? { ...book, highlights: [...book.highlights, text] }
          : book,
      ),
    );
    if (book?.storageKey) {
      void saveAnnotation(book.id, book.currentPage, text, null).catch((error) =>
        setSyncError(messageFromError(error)),
      );
    }
  }

  function addNote(book: ReaderBook, page: number, selectedText: string, note: string) {
    const trimmedNote = note.trim();
    const trimmedSelection = selectedText.trim();
    if (!trimmedNote && !trimmedSelection) return;
    setNotes((current) => [
      {
        id: crypto.randomUUID(),
        bookTitle: book.title,
        page,
        selectedText: trimmedSelection,
        note: trimmedNote,
        createdAt: new Date().toISOString(),
      },
      ...current,
    ]);
    if (book.storageKey) {
      void saveAnnotation(book.id, page, trimmedSelection, trimmedNote).catch((error) =>
        setSyncError(messageFromError(error)),
      );
    }
  }

  return (
    <LibraryContext.Provider
      value={{
        books,
        notes,
        settings,
        syncError,
        isUploading,
        isRefreshing,
        setSettings,
        refreshBooks,
        uploadBook,
        loadBook,
        removeBook,
        renameBook,
        markFinished,
        markOpened,
        updateProgress,
        addHighlight,
        addNote,
      }}
    >
      {children}
    </LibraryContext.Provider>
  );
}

export function useLibrary() {
  const context = useContext(LibraryContext);
  if (!context) {
    throw new Error("useLibrary must be used inside LibraryProvider.");
  }
  return context;
}

function readSavedState() {
  const fallback: {
    notes?: ReaderNote[];
    highlights?: Record<string, string[]>;
    settings?: ReaderSettings;
  } = {};
  if (typeof window === "undefined") return fallback;
  const saved = window.localStorage.getItem(STORAGE_KEY);
  if (!saved) return fallback;
  try {
    const state = JSON.parse(saved) as typeof fallback;
    if (state.settings) {
      state.settings.libraryColumns = clamp(state.settings.libraryColumns ?? 3, 2, 5);
    }
    return state;
  } catch {
    window.localStorage.removeItem(STORAGE_KEY);
    return fallback;
  }
}
