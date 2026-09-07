import type { Metadata } from "next";
import { AppNav } from "./components/AppNav";
import { LibraryProvider } from "./components/LibraryProvider";
import "./globals.css";

export const metadata: Metadata = {
  title: "NXReader",
  description: "A synced web e-reader for EPUB libraries.",
};

export default function RootLayout({ children }: LayoutProps<"/">) {
  return (
    <html lang="en">
      <body>
        <LibraryProvider>
          {children}
          <AppNav />
        </LibraryProvider>
      </body>
    </html>
  );
}
