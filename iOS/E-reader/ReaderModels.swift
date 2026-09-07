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
    @Published var inlineImages: [String: ReaderInlineImage]

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
        highlights: [String] = [],
        inlineImages: [String: ReaderInlineImage] = [:]
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
        self.inlineImages = inlineImages
    }
}

struct ReaderInlineImage {
    let data: Data
    let mimeType: String
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
    @Published var uploadingBook = false

    private let backend = BackendClient()

    init() {
        books = SampleLibrary.books
        notes = SampleLibrary.notes
        readingDays = SampleLibrary.readingDays
        syncError = nil
        downloadingBookIDs = []
        uploadingBook = false
    }

    @MainActor
    func refreshFromBackend() async {
        do {
            let remoteBooks = try await backend.listBooks()
            let existingProgress = Dictionary(uniqueKeysWithValues: books.map {
                ($0.id, ($0.currentPage, $0.pageCount))
            })
            books = remoteBooks.map { backendBook in
                let book = ReaderBook(backendBook: backendBook)
                if let progress = existingProgress[book.id] {
                    book.currentPage = progress.0
                    book.pageCount = progress.1
                }
                return book
            }
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
            book.inlineImages = Dictionary(
                uniqueKeysWithValues: readable.images.compactMap { image in
                    guard let data = Data(base64Encoded: image.dataBase64) else {
                        return nil
                    }
                    return (image.marker, ReaderInlineImage(data: data, mimeType: image.mimeType))
                }
            )
            book.pageCount = max(1, readable.text.count / 1_500)
            syncError = nil
            return true
        } catch {
            syncError = error.localizedDescription
            return false
        }
    }

    @MainActor
    func uploadImportedBook(fileURL: URL) async {
        uploadingBook = true
        defer {
            uploadingBook = false
        }

        do {
            let uploaded = try await backend.uploadBook(fileURL: fileURL)
            let book = ReaderBook(backendBook: uploaded)
            books.removeAll { $0.id == book.id }
            books.insert(book, at: 0)
            syncError = nil
        } catch {
            syncError = error.localizedDescription
        }
    }

    @MainActor
    func loadProgress(_ book: ReaderBook) async {
        guard book.storageKey != nil else {
            return
        }

        do {
            guard let progress = try await backend.progress(for: book) else {
                return
            }
            book.currentPage = max(1, progress.page)
            if let pageCount = progress.pageCount {
                book.pageCount = max(1, pageCount)
            }
            syncError = nil
        } catch {
            syncError = error.localizedDescription
        }
    }

    func saveProgress(_ book: ReaderBook) async {
        guard book.storageKey != nil else {
            return
        }

        do {
            try await backend.saveProgress(
                book: book,
                page: max(1, book.currentPage),
                pageCount: max(1, book.pageCount)
            )
            await MainActor.run {
                syncError = nil
            }
        } catch {
            await MainActor.run {
                syncError = error.localizedDescription
            }
        }
    }

    @MainActor
    func removeBook(_ book: ReaderBook) async {
        do {
            if book.storageKey != nil {
                try await backend.deleteBook(book)
            }
            try? BookCache.remove(book)
            books.removeAll { $0.id == book.id }
            syncError = nil
        } catch {
            syncError = error.localizedDescription
        }
    }

    @MainActor
    func renameBook(_ book: ReaderBook, title: String) {
        let trimmed = title.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            return
        }
        book.title = trimmed
    }

    @MainActor
    func markFinished(_ book: ReaderBook) {
        book.pageCount = max(1, book.pageCount)
        book.currentPage = book.pageCount
        Task {
            await saveProgress(book)
        }
    }

    @MainActor
    func markOpened(_ book: ReaderBook) {
        book.lastOpenedAt = .now
        books.sort { $0.lastOpenedAt > $1.lastOpenedAt }
    }

    @MainActor
    func addNote(book: ReaderBook, page: Int, selectedText: String, note: String) {
        let trimmedNote = note.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedSelection = selectedText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedNote.isEmpty || !trimmedSelection.isEmpty else {
            return
        }

        notes.insert(
            ReaderNote(bookTitle: book.title, page: page, selectedText: trimmedSelection, note: trimmedNote),
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
