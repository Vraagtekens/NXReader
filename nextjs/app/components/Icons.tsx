export function HomeIcon() {
  return (
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M4 11.4 12 5l8 6.4" />
      <path d="M6.5 10.2V20h11v-9.8" />
      <path d="M10 20v-5h4v5" />
    </svg>
  );
}

export function LibraryIcon() {
  return (
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M5.2 5.7h5.15c1 0 1.8.8 1.8 1.8v11.1c-.42-.7-1.08-1.05-1.98-1.05H5.2z" />
      <path d="M18.8 5.7h-5.15c-1 0-1.8.8-1.8 1.8v11.1c.42-.7 1.08-1.05 1.98-1.05h4.97z" />
      <path d="M12 7.5v11.1" />
    </svg>
  );
}

export function NotesIcon() {
  return (
    <svg viewBox="0 0 24 24" aria-hidden="true">
      <path d="M7 7h10" />
      <path d="M7 11h7" />
      <path d="M7 15h5" />
      <path d="M5 3.5h14A1.5 1.5 0 0 1 20.5 5v14A1.5 1.5 0 0 1 19 20.5H5A1.5 1.5 0 0 1 3.5 19V5A1.5 1.5 0 0 1 5 3.5z" />
    </svg>
  );
}

export function DensityIcon({ columns }: { columns: number }) {
  return (
    <svg className="density-icon" viewBox="0 0 22 22" aria-hidden="true">
      {Array.from({ length: columns }).map((_, index) => {
        const gap = 1.5;
        const width = (16 - gap * (columns - 1)) / columns;
        return (
          <rect
            key={index}
            x={3 + index * (width + gap)}
            y="5"
            width={width}
            height="12"
            rx="1"
          />
        );
      })}
    </svg>
  );
}
