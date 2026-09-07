"use client";

import { useEffect, useMemo, useState } from "react";
import { useRouter } from "next/navigation";
import { BookCover } from "./BookCover";
import { RenderedText } from "./RenderedText";
import { useLibrary } from "./LibraryProvider";
import { buildChapters, paginate } from "../lib/text";
import { ReaderBook, ReaderFont } from "../lib/types";
import { clamp } from "../lib/books";

export function ReaderScreen({ bookId }: { bookId: string }) {
  const {
    books,
    settings,
    setSettings,
    loadBook,
    updateProgress,
    addHighlight,
    addNote,
  } = useLibrary();
  const router = useRouter();
  const book = books.find((item) => item.id === bookId);

  useEffect(() => {
    void loadBook(bookId);
  }, [bookId, loadBook]);

  if (!book) {
    return (
      <main className="reader-shell light">
        <div className="reader-loading">Opening book…</div>
      </main>
    );
  }

  return (
    <Reader
      book={book}
      onBack={() => router.push("/library")}
      settings={settings}
      setSettings={setSettings}
      onProgress={updateProgress}
      onHighlight={addHighlight}
      onNote={addNote}
    />
  );
}

function Reader({
  book,
  settings,
  setSettings,
  onBack,
  onProgress,
  onHighlight,
  onNote,
}: {
  book: ReaderBook;
  settings: ReturnType<typeof useLibrary>["settings"];
  setSettings: ReturnType<typeof useLibrary>["setSettings"];
  onBack: () => void;
  onProgress: (bookId: string, page: number, pageCount: number) => void;
  onHighlight: (bookId: string, selectedText: string) => void;
  onNote: (book: ReaderBook, page: number, selectedText: string, note: string) => void;
}) {
  const [chromeVisible, setChromeVisible] = useState(true);
  const [selectedText, setSelectedText] = useState("");
  const [sheet, setSheet] = useState<"contents" | "settings" | "note" | null>(null);
  const [noteDraft, setNoteDraft] = useState("");
  const pages = useMemo(
    () => paginate(book.sampleText, settings.fontSize),
    [book.sampleText, settings.fontSize],
  );
  const page = clamp(book.currentPage, 1, pages.length);
  const currentPageText = pages[page - 1] ?? "";
  const chapters = useMemo(() => buildChapters(pages), [pages]);

  useEffect(() => {
    if (book.pageCount !== pages.length) {
      onProgress(book.id, page, pages.length);
    }
  }, [book.id, book.pageCount, onProgress, page, pages.length]);

  function changePage(nextPage: number) {
    onProgress(book.id, clamp(nextPage, 1, pages.length), pages.length);
    setSelectedText("");
  }

  function captureSelection() {
    const selection = window.getSelection()?.toString() ?? "";
    setSelectedText(selection);
  }

  function saveNote() {
    onNote(book, page, selectedText, noteDraft);
    setNoteDraft("");
    setSelectedText("");
    setSheet(null);
  }

  return (
    <main className={`reader-shell ${settings.theme}`}>
      {chromeVisible && (
        <header className="reader-top">
          <button title="Back" onClick={onBack}>
            ‹
          </button>
          <span>{book.title}</span>
          <i />
        </header>
      )}

      <section
        className={`reader-page font-${settings.fontFamily}`}
        style={{ fontSize: settings.fontSize }}
        onMouseUp={captureSelection}
        onTouchEnd={captureSelection}
        onClick={() => setChromeVisible((visible) => !visible)}
      >
        {page === 1 && currentPageText.trim().length < 80 ? (
          <div className="reader-cover-page">
            <BookCover book={book} size="large" />
            <strong>{book.title}</strong>
            <span>{book.author}</span>
          </div>
        ) : (
          <RenderedText text={currentPageText} highlights={book.highlights} images={book.images} />
        )}
      </section>

      {chromeVisible && (
        <footer className="reader-bottom">
          <button title="Previous page" onClick={() => changePage(page - 1)} disabled={page === 1}>
            ‹
          </button>
          <span>
            Page {page} of {pages.length}
          </span>
          <button
            title="Next page"
            onClick={() => changePage(page + 1)}
            disabled={page === pages.length}
          >
            ›
          </button>
          <div className="reader-menu">
            {selectedText.trim() && (
              <>
                <button title="Highlight" onClick={() => onHighlight(book.id, selectedText)}>
                  H
                </button>
                <button title="Note" onClick={() => setSheet("note")}>
                  ✎
                </button>
              </>
            )}
            <button title="Contents" onClick={() => setSheet("contents")}>
              ≡
            </button>
            <button title="Settings" onClick={() => setSheet("settings")}>
              A
            </button>
          </div>
        </footer>
      )}

      {sheet && (
        <div className="sheet-backdrop" onClick={() => setSheet(null)}>
          <aside className="reader-sheet" onClick={(event) => event.stopPropagation()}>
            <button className="sheet-close" title="Close" onClick={() => setSheet(null)}>
              ×
            </button>
            {sheet === "contents" && (
              <>
                <h2>Contents</h2>
                <div className="contents-list">
                  {chapters.map((chapter) => (
                    <button
                      key={`${chapter.title}-${chapter.page}`}
                      onClick={() => {
                        changePage(chapter.page);
                        setSheet(null);
                      }}
                    >
                      <span>{chapter.title}</span>
                      <small>Page {chapter.page}</small>
                    </button>
                  ))}
                </div>
              </>
            )}
            {sheet === "settings" && (
              <>
                <h2>Theme & Settings</h2>
                <label>
                  Font size
                  <input
                    type="range"
                    min="16"
                    max="34"
                    value={settings.fontSize}
                    onChange={(event) =>
                      setSettings({ ...settings, fontSize: Number(event.target.value) })
                    }
                  />
                </label>
                <label>
                  Font
                  <select
                    value={settings.fontFamily}
                    onChange={(event) =>
                      setSettings({ ...settings, fontFamily: event.target.value as ReaderFont })
                    }
                  >
                    <option value="georgia">Georgia</option>
                    <option value="atkinson">Atkinson</option>
                    <option value="lexend">Lexend</option>
                    <option value="dyslexic">OpenDyslexic</option>
                  </select>
                </label>
                <label className="toggle-row">
                  Dark Mode
                  <input
                    type="checkbox"
                    checked={settings.theme === "dark"}
                    onChange={(event) =>
                      setSettings({
                        ...settings,
                        theme: event.target.checked ? "dark" : "light",
                      })
                    }
                  />
                </label>
              </>
            )}
            {sheet === "note" && (
              <>
                <h2>Add Note</h2>
                <blockquote>{selectedText}</blockquote>
                <textarea
                  value={noteDraft}
                  onChange={(event) => setNoteDraft(event.target.value)}
                  placeholder="Write a note"
                />
                <button className="primary-action" onClick={saveNote} disabled={!noteDraft.trim()}>
                  Save
                </button>
              </>
            )}
          </aside>
        </div>
      )}
    </main>
  );
}
