"use client";

import { useLibrary } from "./LibraryProvider";

export function NotesScreen() {
  const { notes } = useLibrary();

  return (
    <div className="screen">
      <h1>Notes</h1>
      <div className="notes-list">
        {notes.map((note) => (
          <article className="note-card" key={note.id}>
            <header>
              <strong>{note.bookTitle}</strong>
              <span>p. {note.page}</span>
            </header>
            <p className="selection">{note.selectedText}</p>
            {note.note && <p>{note.note}</p>}
          </article>
        ))}
      </div>
    </div>
  );
}
