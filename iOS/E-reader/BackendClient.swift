import Foundation

struct BackendBook: Decodable {
    let id: UUID
    let contentHash: String
    let title: String
    let author: String?
    let fileName: String?
    let storageKey: String?
    let mimeType: String?
    let fileSizeBytes: Int64?
    let createdAt: Date
    let updatedAt: Date
}

struct BackendBookRead: Decodable {
    let bookId: UUID
    let title: String
    let text: String
}

enum BackendClientError: LocalizedError {
    case missingAPIKey
    case badStatus(Int)
    case invalidResponse

    var errorDescription: String? {
        switch self {
        case .missingAPIKey:
            "Set AppConfig.apiKey before syncing."
        case .badStatus(let status):
            "Backend returned HTTP \(status)."
        case .invalidResponse:
            "Backend returned an invalid response."
        }
    }
}

final class BackendClient {
    private let baseURL: URL
    private let apiKey: String
    private let session: URLSession
    private let decoder: JSONDecoder

    init(
        baseURL: URL = AppConfig.apiBaseURL,
        apiKey: String = AppConfig.apiKey,
        session: URLSession = .shared
    ) {
        self.baseURL = baseURL
        self.apiKey = apiKey
        self.session = session

        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .custom { decoder in
            let container = try decoder.singleValueContainer()
            let value = try container.decode(String.self)
            if let date = BackendDateParser.parse(value) {
                return date
            }
            throw DecodingError.dataCorruptedError(
                in: container,
                debugDescription: "Invalid date: \(value)"
            )
        }
        self.decoder = decoder
    }

    func listBooks() async throws -> [BackendBook] {
        let data = try await data(path: "/books")
        return try decoder.decode([BackendBook].self, from: data)
    }

    func downloadBook(_ book: ReaderBook) async throws -> URL {
        let destination = try BookCache.cachedURL(for: book)
        if FileManager.default.fileExists(atPath: destination.path) {
            return destination
        }

        let data = try await data(path: "/books/\(book.id.uuidString)/download")
        try BookCache.store(data, for: book)
        return destination
    }

    func readBook(_ book: ReaderBook) async throws -> BackendBookRead {
        let data = try await data(path: "/books/\(book.id.uuidString)/read")
        return try decoder.decode(BackendBookRead.self, from: data)
    }

    func coverImageData(_ book: ReaderBook) async throws -> Data {
        try await data(path: "/books/\(book.id.uuidString)/cover")
    }

    private func data(path: String) async throws -> Data {
        guard apiKey != "replace-with-your-api-key", !apiKey.isEmpty else {
            throw BackendClientError.missingAPIKey
        }

        let url = baseURL.appendingPathComponent(path.trimmingCharacters(in: CharacterSet(charactersIn: "/")))
        var request = URLRequest(url: url)
        request.setValue(apiKey, forHTTPHeaderField: "x-api-key")

        let (data, response) = try await session.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse else {
            throw BackendClientError.invalidResponse
        }
        guard (200..<300).contains(httpResponse.statusCode) else {
            throw BackendClientError.badStatus(httpResponse.statusCode)
        }

        return data
    }
}

enum BackendDateParser {
    private static let fractionalFormatter: ISO8601DateFormatter = {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return formatter
    }()

    private static let formatter: ISO8601DateFormatter = {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime]
        return formatter
    }()

    static func parse(_ value: String) -> Date? {
        fractionalFormatter.date(from: value) ?? formatter.date(from: value)
    }
}

enum BookCache {
    static func cachedURL(for book: ReaderBook) throws -> URL {
        try cachedURL(forID: book.id)
    }

    static func cachedURL(forID id: UUID) throws -> URL {
        try directory()
            .appendingPathComponent(id.uuidString, isDirectory: false)
            .appendingPathExtension("epub")
    }

    @discardableResult
    static func store(_ data: Data, for book: ReaderBook) throws -> URL {
        let url = try cachedURL(for: book)
        try data.write(to: url, options: .atomic)
        return url
    }

    private static func directory() throws -> URL {
        let caches = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
        let directory = caches.appendingPathComponent("Books", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory
    }
}
