export type ReaderTheme = "dark" | "light";
export type ReaderFont = "georgia" | "atkinson" | "lexend" | "dyslexic";

export type BackendBook = {
  id: string;
  contentHash: string;
  title: string;
  author?: string | null;
  fileName?: string | null;
  storageKey?: string | null;
  coverStorageKey?: string | null;
  mimeType?: string | null;
  fileSizeBytes?: number | null;
  createdAt: string;
  updatedAt: string;
};

export type BackendReadImage = {
  marker: string;
  mimeType: string;
  dataBase64: string;
};

export type BackendBookRead = {
  bookId: string;
  title: string;
  text: string;
  images: BackendReadImage[];
};

export type ReadingProgress = {
  bookId: string;
  page: number;
  pageCount?: number | null;
  progressPercent?: number | null;
};

export type BackendAnnotation = {
  id: string;
  bookId: string;
  page: number;
  selectedText: string;
  translation?: string | null;
  note?: string | null;
  createdAt: string;
  updatedAt: string;
};

export type ReaderBook = {
  id: string;
  title: string;
  author: string;
  fileName?: string | null;
  storageKey?: string | null;
  coverColorHex: string;
  currentPage: number;
  pageCount: number;
  lastOpenedAt: string;
  sampleText: string;
  images: BackendReadImage[];
  highlights: string[];
};

export type ReaderNote = {
  id: string;
  bookTitle: string;
  page: number;
  selectedText: string;
  note: string;
  createdAt: string;
};

export type ReaderSettings = {
  fontSize: number;
  fontFamily: ReaderFont;
  theme: ReaderTheme;
  libraryColumns: number;
};
