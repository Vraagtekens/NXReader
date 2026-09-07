"use client";

import { useParams } from "next/navigation";
import { ReaderScreen } from "../../components/ReaderScreen";

export default function ReaderPage() {
  const params = useParams<{ bookId: string }>();
  return <ReaderScreen bookId={params.bookId} />;
}
