import { ReaderBook, ReaderNote } from "./types";

export const sampleText = `# Grand titre H1: Elevation

Ce paragraphe teste les accents francais: poete, peche, foret, coeur, deja, ou, ca, Noel, garcon.

## Sous-titre H2: Au lecteur

Texte normal, puis **texte en gras**, puis _texte en italique_, puis **_gras italique_**.

Variantes avec balises courtes: **b en gras**, _i en italique_, et **_b plus i ensemble_**.

### Titre H3: Dialogue

Bonjour, dit-elle. C'est une phrase avec des guillemets francais, une apostrophe, et un tiret simple - pour tester le rendu.

Une citation longue devrait avoir un style distinct plus tard. Pour l'instant, elle aide a voir si le texte reste lisible.

- premier element avec _italique_.

- deuxieme element avec **gras**.

Liste: troisieme element avec petites capitales.

Longue ligne pour tester le passage a la ligne: le mot selectionne a la fin d'une phrase devrait rester touchable meme quand il descend sur la ligne suivante.`;

export const sampleBooks: ReaderBook[] = [
  {
    id: "sample-typography",
    title: "Typography Test",
    author: "NXReader",
    fileName: "typography-test.epub",
    coverColorHex: "3454D1",
    currentPage: 1,
    pageCount: 4,
    lastOpenedAt: new Date().toISOString(),
    sampleText,
    images: [],
    highlights: [],
  },
  {
    id: "sample-margins",
    title: "Margins",
    author: "D. Jansen",
    coverColorHex: "1F8A70",
    currentPage: 11,
    pageCount: 149,
    lastOpenedAt: addDays(-3).toISOString(),
    sampleText,
    images: [],
    highlights: [],
  },
  {
    id: "sample-night-library",
    title: "Night Library",
    author: "Homebrew Press",
    coverColorHex: "8A4FFF",
    currentPage: 87,
    pageCount: 301,
    lastOpenedAt: addDays(-9).toISOString(),
    sampleText,
    images: [],
    highlights: [],
  },
];

export const sampleNotes: ReaderNote[] = [
  {
    id: "note-typography",
    bookTitle: "Typography Test",
    page: 1,
    selectedText: "Texte normal, puis texte en gras",
    note: "Use this to compare iOS typography rendering with NXReader.",
    createdAt: new Date().toISOString(),
  },
  {
    id: "note-margins",
    bookTitle: "Margins",
    page: 8,
    selectedText: "new streets, new rooms, new voices",
    note: "Use this as onboarding copy later.",
    createdAt: addDays(-1).toISOString(),
  },
];

function addDays(days: number) {
  const date = new Date();
  date.setDate(date.getDate() + days);
  return date;
}
