"use client";

import { CSSProperties, useRef } from "react";
import Link from "next/link";
import { BookCover } from "./BookCover";
import { DensityIcon } from "./Icons";
import { useLibrary } from "./LibraryProvider";
import { progressLabel } from "../lib/books";

export function LibraryScreen() {
  const {
    books,
    settings,
    syncError,
    isUploading,
    isRefreshing,
    setSettings,
    uploadBook,
    refreshBooks,
    loadBook,
    removeBook,
    renameBook,
    markFinished,
  } = useLibrary();
  const inputRef = useRef<HTMLInputElement>(null);
  const columns = settings.libraryColumns;

  function cycleColumns() {
    const next = columns >= 5 ? 2 : columns + 1;
    setSettings({ ...settings, libraryColumns: next });
  }

  return (
    <div className="screen">
      <header className="screen-header">
        <h1>Library</h1>
        <div className="round-actions">
          <button className="density-button" title={`${columns} books per row`} onClick={cycleColumns}>
            <DensityIcon columns={columns} />
            <span>{columns}</span>
          </button>
          <button title="Refresh" onClick={refreshBooks} disabled={isRefreshing}>
            {isRefreshing ? "…" : "↻"}
          </button>
          <button title="Add EPUB" onClick={() => inputRef.current?.click()} disabled={isUploading}>
            {isUploading ? "…" : "+"}
          </button>
          <input ref={inputRef} type="file" accept=".epub" hidden onChange={uploadBook} />
        </div>
      </header>

      {syncError && <p className="sync-error">{syncError}</p>}

      <div className="book-grid" style={{ "--library-columns": columns } as CSSProperties}>
        {books.map((book) => (
          <article className="book-tile" key={book.id}>
            <Link className="book-open" href={`/reader/${book.id}`} onClick={() => void loadBook(book.id)}>
              <BookCover book={book} size="small" />
            </Link>
            <div className="tile-meta">
              <span>{progressLabel(book)}</span>
              {!book.storageKey && <span title="Local sample">○</span>}
              <div className="tile-menu">
                <button title="Rename" onClick={() => renameBook(book)}>
                  ✎
                </button>
                <button title="Mark as Finished" onClick={() => markFinished(book)}>
                  ✓
                </button>
                <button title="Remove" onClick={() => void removeBook(book)}>
                  ×
                </button>
              </div>
            </div>
          </article>
        ))}
      </div>
    </div>
  );
}
