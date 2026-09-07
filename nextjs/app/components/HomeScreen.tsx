"use client";

import Link from "next/link";
import { BookCover } from "./BookCover";
import { useLibrary } from "./LibraryProvider";
import { clamp, progressLabel, progressPercent } from "../lib/books";

export function HomeScreen() {
  const { books, loadBook } = useLibrary();
  const recent = [...books].sort(
    (a, b) => Date.parse(b.lastOpenedAt) - Date.parse(a.lastOpenedAt),
  )[0];

  return (
    <div className="screen">
      <h1>Home</h1>
      {recent ? (
        <Link className="recent-card" href={`/reader/${recent.id}`} onClick={() => void loadBook(recent.id)}>
          <BookCover book={recent} size="large" />
          <div className="recent-details">
            <span>Latest Book</span>
            <strong>{recent.title}</strong>
            <p>{recent.author}</p>
            <div className="progress-row">
              <b>{progressLabel(recent)}</b>
              <small>
                Page {clamp(recent.currentPage, 1, recent.pageCount)} / {recent.pageCount}
              </small>
            </div>
            <div className="meter">
              <i style={{ width: `${progressPercent(recent)}%` }} />
            </div>
          </div>
        </Link>
      ) : (
        <div className="empty-state">
          <strong>No Books Yet</strong>
          <span>Add an EPUB from the Library tab.</span>
        </div>
      )}
    </div>
  );
}
