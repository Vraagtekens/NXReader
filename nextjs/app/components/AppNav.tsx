"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { ReactNode } from "react";
import { HomeIcon, LibraryIcon, NotesIcon } from "./Icons";

const tabs = [
  { href: "/", label: "Home", icon: <HomeIcon /> },
  { href: "/library", label: "Library", icon: <LibraryIcon /> },
  { href: "/notes", label: "Notes", icon: <NotesIcon /> },
];

export function AppNav() {
  const pathname = usePathname();
  if (pathname.startsWith("/reader/")) return null;

  return (
    <nav className="tab-bar" aria-label="Reader sections">
      {tabs.map((tab) => (
        <TabLink
          key={tab.href}
          href={tab.href}
          icon={tab.icon}
          label={tab.label}
          isActive={tab.href === "/" ? pathname === "/" : pathname.startsWith(tab.href)}
        />
      ))}
    </nav>
  );
}

function TabLink({
  href,
  icon,
  label,
  isActive,
}: {
  href: string;
  icon: ReactNode;
  label: string;
  isActive: boolean;
}) {
  return (
    <Link className={isActive ? "active" : ""} href={href}>
      <span>{icon}</span>
      {label}
    </Link>
  );
}
