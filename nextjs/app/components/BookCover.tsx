"use client";

import { useEffect, useState } from "react";
import { coverBlob } from "../lib/backend";
import { ReaderBook } from "../lib/types";

export function BookCover({ book, size }: { book: ReaderBook; size: "small" | "large" }) {
  const [coverUrl, setCoverUrl] = useState<string | null>(null);

  useEffect(() => {
    if (!book.storageKey) return;
    const controller = new AbortController();
    let objectUrl: string | null = null;
    coverBlob(book.id, controller.signal)
      .then((blob) => {
        const url = URL.createObjectURL(blob);
        objectUrl = url;
        setCoverUrl(url);
      })
      .catch(() => setCoverUrl(null));
    return () => {
      controller.abort();
      if (objectUrl) URL.revokeObjectURL(objectUrl);
    };
  }, [book.id, book.storageKey]);

  return (
    <div
      className={`book-cover ${size}`}
      style={{
        background: `linear-gradient(135deg, #${book.coverColorHex}, #${book.coverColorHex}99, #00000048)`,
      }}
    >
      {coverUrl ? (
        // eslint-disable-next-line @next/next/no-img-element
        <img src={coverUrl} alt="" />
      ) : (
        <div>
          <strong>{book.title}</strong>
          <span>{book.author}</span>
        </div>
      )}
    </div>
  );
}
