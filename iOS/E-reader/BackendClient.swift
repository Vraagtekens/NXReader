import Foundation

struct BackendBook: Decodable {
    let id: UUID
    let contentHash: String
    let title: String
    let author: String?
    let fileName: String?
    let storageKey: String?
    let coverStorageKey: String?
    let mimeType: String?
    let fileSizeBytes: Int64?
    let createdAt: Date
    let updatedAt: Date
}

struct BackendBookRead: Decodable {
    let bookId: UUID
    let title: String
    let text: String
    let images: [BackendBookReadImage]

    enum CodingKeys: String, CodingKey {
        case bookId
        case title
        case text
        case images
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        bookId = try container.decode(UUID.self, forKey: .bookId)
        title = try container.decode(String.self, forKey: .title)
        text = try container.decode(String.self, forKey: .text)
        images = try container.decodeIfPresent([BackendBookReadImage].self, forKey: .images) ?? []
    }
}

struct BackendBookReadImage: Decodable {
    let marker: String
    let mimeType: String
    let dataBase64: String
}

struct BackendReadingProgress: Decodable {
    let bookId: UUID
    let page: Int
    let pageCount: Int?
    let progressPercent: Double?
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

    func uploadBook(fileURL: URL) async throws -> BackendBook {
        let fileData = try Data(contentsOf: fileURL)
        let fileName = fileURL.lastPathComponent
        let title = (fileName as NSString).deletingPathExtension
        let boundary = "Boundary-\(UUID().uuidString)"
        var body = Data()

        body.appendMultipartField(name: "title", value: title, boundary: boundary)
        body.appendMultipartFile(
            name: "file",
            fileName: fileName,
            mimeType: "application/epub+zip",
            data: fileData,
            boundary: boundary
        )
        body.appendString("--\(boundary)--\r\n")

        var request = try request(path: "/books/upload")
        request.httpMethod = "POST"
        request.setValue("multipart/form-data; boundary=\(boundary)", forHTTPHeaderField: "Content-Type")
        request.setValue(String(body.count), forHTTPHeaderField: "Content-Length")

        let data = try await responseData(for: request, body: body)
        return try decoder.decode(BackendBook.self, from: data)
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

    func deleteBook(_ book: ReaderBook) async throws {
        var deleteRequest = try request(path: "/books/\(book.id.uuidString)")
        deleteRequest.httpMethod = "DELETE"
        do {
            _ = try await responseData(for: deleteRequest)
        } catch BackendClientError.badStatus(405) {
            var fallbackRequest = try request(path: "/books/\(book.id.uuidString)/delete")
            fallbackRequest.httpMethod = "POST"
            _ = try await responseData(for: fallbackRequest)
        }
    }

    func progress(for book: ReaderBook) async throws -> BackendReadingProgress? {
        do {
            let data = try await data(path: "/books/\(book.id.uuidString)/progress")
            return try decoder.decode(BackendReadingProgress.self, from: data)
        } catch BackendClientError.badStatus(404) {
            return nil
        }
    }

    func saveProgress(book: ReaderBook, page: Int, pageCount: Int) async throws {
        let progressPercent = pageCount > 1
            ? (Double(max(1, page) - 1) / Double(pageCount - 1)) * 100
            : 0.0
        let payload: [String: Any?] = [
            "page": page,
            "pageCount": pageCount,
            "progressPercent": progressPercent,
            "locator": ["page": page]
        ]
        let body = try JSONSerialization.data(withJSONObject: payload.compactMapValues { $0 })

        var request = try request(path: "/books/\(book.id.uuidString)/progress")
        request.httpMethod = "PUT"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        _ = try await responseData(for: request, body: body)
    }

    private func request(path: String) throws -> URLRequest {
        guard apiKey != "replace-with-your-api-key", !apiKey.isEmpty else {
            throw BackendClientError.missingAPIKey
        }

        let url = baseURL.appendingPathComponent(path.trimmingCharacters(in: CharacterSet(charactersIn: "/")))
        var request = URLRequest(url: url)
        request.setValue(apiKey, forHTTPHeaderField: "x-api-key")
        return request
    }

    private func data(path: String) async throws -> Data {
        let request = try request(path: path)
        return try await responseData(for: request)
    }

    private func responseData(for request: URLRequest, body: Data? = nil) async throws -> Data {
        let dataAndResponse: (Data, URLResponse)
        if let body {
            dataAndResponse = try await session.upload(for: request, from: body)
        } else {
            dataAndResponse = try await session.data(for: request)
        }

        let (data, response) = dataAndResponse
        guard let httpResponse = response as? HTTPURLResponse else {
            throw BackendClientError.invalidResponse
        }
        guard (200..<300).contains(httpResponse.statusCode) else {
            throw BackendClientError.badStatus(httpResponse.statusCode)
        }

        return data
    }
}

private extension Data {
    mutating func appendString(_ string: String) {
        append(Data(string.utf8))
    }

    mutating func appendMultipartField(name: String, value: String, boundary: String) {
        appendString("--\(boundary)\r\n")
        appendString("Content-Disposition: form-data; name=\"\(name)\"\r\n\r\n")
        appendString("\(value)\r\n")
    }

    mutating func appendMultipartFile(
        name: String,
        fileName: String,
        mimeType: String,
        data: Data,
        boundary: String
    ) {
        appendString("--\(boundary)\r\n")
        appendString("Content-Disposition: form-data; name=\"\(name)\"; filename=\"\(fileName)\"\r\n")
        appendString("Content-Type: \(mimeType)\r\n\r\n")
        append(data)
        appendString("\r\n")
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

    static func remove(_ book: ReaderBook) throws {
        let url = try cachedURL(for: book)
        if FileManager.default.fileExists(atPath: url.path) {
            try FileManager.default.removeItem(at: url)
        }
    }

    private static func directory() throws -> URL {
        let caches = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
        let directory = caches.appendingPathComponent("Books", isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory
    }
}
