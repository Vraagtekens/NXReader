import Combine
import Foundation

final class ReaderBook: ObservableObject, Identifiable {
    let id: UUID
    @Published var title: String
    @Published var author: String
    @Published var fileName: String?
    @Published var storageKey: String?
    @Published var isDownloaded: Bool
    @Published var coverColorHex: String
    @Published var currentPage: Int
    @Published var currentPart: Int
    @Published var pageCount: Int
    @Published var highlights: [String]
    @Published var lastOpenedAt: Date
    let createdAt: Date
    @Published var sampleText: String

    init(
        id: UUID = UUID(),
        title: String,
        author: String,
        fileName: String? = nil,
        storageKey: String? = nil,
        isDownloaded: Bool = false,
        coverColorHex: String,
        currentPage: Int = 1,
        currentPart: Int = 1,
        pageCount: Int = 1,
        lastOpenedAt: Date = .now,
        createdAt: Date = .now,
        sampleText: String,
        highlights: [String] = []
    ) {
        self.id = id
        self.title = title
        self.author = author
        self.fileName = fileName
        self.storageKey = storageKey
        self.isDownloaded = isDownloaded
        self.coverColorHex = coverColorHex
        self.currentPage = currentPage
        self.currentPart = currentPart
        self.pageCount = max(1, pageCount)
        self.lastOpenedAt = lastOpenedAt
        self.createdAt = createdAt
        self.sampleText = sampleText
        self.highlights = highlights
    }
}

struct ReaderNote: Identifiable {
    let id: UUID
    let bookTitle: String
    let page: Int
    let selectedText: String
    let note: String
    let createdAt: Date

    init(
        id: UUID = UUID(),
        bookTitle: String,
        page: Int,
        selectedText: String,
        note: String,
        createdAt: Date = .now
    ) {
        self.id = id
        self.bookTitle = bookTitle
        self.page = page
        self.selectedText = selectedText
        self.note = note
        self.createdAt = createdAt
    }
}

struct ReadingDay: Identifiable {
    var id: Date { date }
    let date: Date
    let minutesRead: Int

    init(date: Date, minutesRead: Int) {
        self.date = Calendar.current.startOfDay(for: date)
        self.minutesRead = minutesRead
    }
}

final class LibraryStore: ObservableObject {
    @Published var books: [ReaderBook]
    @Published var notes: [ReaderNote]
    @Published var readingDays: [ReadingDay]
    @Published var syncError: String?
    @Published var downloadingBookIDs: Set<UUID>

    private let backend = BackendClient()

    init() {
        books = SampleLibrary.books
        notes = SampleLibrary.notes
        readingDays = SampleLibrary.readingDays
        syncError = nil
        downloadingBookIDs = []
    }

    @MainActor
    func refreshFromBackend() async {
        do {
            let remoteBooks = try await backend.listBooks()
            books = remoteBooks.map(ReaderBook.init(backendBook:))
            syncError = nil
        } catch {
            syncError = error.localizedDescription
        }
    }

    @MainActor
    func ensureDownloaded(_ book: ReaderBook) async -> URL? {
        if let cachedURL = try? BookCache.cachedURL(for: book),
           FileManager.default.fileExists(atPath: cachedURL.path) {
            book.isDownloaded = true
            return cachedURL
        }

        downloadingBookIDs.insert(book.id)
        defer {
            downloadingBookIDs.remove(book.id)
        }

        do {
            let url = try await backend.downloadBook(book)
            book.isDownloaded = true
            syncError = nil
            return url
        } catch {
            syncError = error.localizedDescription
            return nil
        }
    }

    @MainActor
    func loadReadableText(_ book: ReaderBook) async -> Bool {
        guard book.storageKey != nil else {
            return true
        }

        do {
            let readable = try await backend.readBook(book)
            book.sampleText = readable.text
            book.pageCount = max(1, readable.text.count / 1_500)
            syncError = nil
            return true
        } catch {
            syncError = error.localizedDescription
            return false
        }
    }

    func addImportedBook(fileName: String) {
        let title = (fileName as NSString).deletingPathExtension
        let book = ReaderBook(
            title: title,
            author: "Unknown Author",
            fileName: fileName,
            coverColorHex: ["3454D1", "1F8A70", "C44536", "8A4FFF"].randomElement() ?? "3454D1",
            currentPage: 1,
            pageCount: 1,
            sampleText: SampleLibrary.readerText
        )
        books.insert(book, at: 0)
    }

    func addNote(book: ReaderBook, page: Int, selectedText: String, note: String) {
        notes.insert(
            ReaderNote(bookTitle: book.title, page: page, selectedText: selectedText, note: note),
            at: 0
        )
    }
}

private extension ReaderBook {
    convenience init(backendBook: BackendBook) {
        self.init(
            id: backendBook.id,
            title: backendBook.title,
            author: backendBook.author ?? "Unknown Author",
            fileName: backendBook.fileName,
            storageKey: backendBook.storageKey,
            isDownloaded: (try? BookCache.cachedURL(forID: backendBook.id).checkResourceIsReachable()) ?? false,
            coverColorHex: Self.coverColor(for: backendBook.contentHash),
            currentPage: 1,
            pageCount: 1,
            lastOpenedAt: backendBook.updatedAt,
            createdAt: backendBook.createdAt,
            sampleText: SampleLibrary.readerText
        )
    }

    static func coverColor(for value: String) -> String {
        let colors = ["3454D1", "1F8A70", "C44536", "8A4FFF", "2F4858"]
        let sum = value.unicodeScalars.reduce(0) { $0 + Int($1.value) }
        return colors[sum % colors.count]
    }
}

enum SampleLibrary {
    static let readerText = """
    # Grand titre H1: Elevation

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

    Longue ligne pour tester le passage a la ligne: le mot selectionne a la fin d'une phrase devrait rester touchable meme quand il descend sur la ligne suivante.
    """

    static var books: [ReaderBook] {
        let calendar = Calendar.current
        return [
            ReaderBook(
                title: "Typography Test",
                author: "NXReader",
                fileName: "typography-test.epub",
                coverColorHex: "3454D1",
                currentPage: 1,
                pageCount: 4,
                lastOpenedAt: .now,
                sampleText: readerText
            ),
            ReaderBook(
                title: "Margins",
                author: "D. Jansen",
                coverColorHex: "1F8A70",
                currentPage: 11,
                pageCount: 149,
                lastOpenedAt: calendar.date(byAdding: .day, value: -3, to: .now) ?? .now,
                sampleText: readerText
            ),
            ReaderBook(
                title: "Night Library",
                author: "Homebrew Press",
                coverColorHex: "8A4FFF",
                currentPage: 87,
                pageCount: 301,
                lastOpenedAt: calendar.date(byAdding: .day, value: -9, to: .now) ?? .now,
                sampleText: readerText
            )
        ]

    }

    static var notes: [ReaderNote] {
        [
            ReaderNote(bookTitle: "Typography Test", page: 1, selectedText: "Texte normal, puis texte en gras", note: "Use this to compare iOS typography rendering with NXReader."),
            ReaderNote(bookTitle: "Margins", page: 8, selectedText: "new streets, new rooms, new voices", note: "Use this as onboarding copy later.")
        ]
    }

    static var readingDays: [ReadingDay] {
        let calendar = Calendar.current
        var days: [ReadingDay] = []
        for offset in 0..<31 {
            guard let date = calendar.date(byAdding: .day, value: -offset, to: .now) else { continue }
            let minutes = [0, 8, 14, 22, 35, 48, 63][offset % 7]
            days.append(ReadingDay(date: date, minutesRead: minutes))
        }
        return days.sorted { $0.date < $1.date }
    }
}
