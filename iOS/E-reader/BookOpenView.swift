import SwiftUI

struct BookOpenView: View {
    @ObservedObject var book: ReaderBook
    @ObservedObject var store: LibraryStore
    @State private var cachedURL: URL?
    @State private var isReadable = false
    @State private var didTryDownload = false

    var body: some View {
        Group {
            if isReadable {
                ReaderView(book: book, store: store)
            } else {
                VStack(spacing: 14) {
                    ProgressView()
                    Text(statusText)
                        .font(.headline)
                    if let syncError = store.syncError, didTryDownload {
                        Text(syncError)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Color(.systemBackground))
                .task {
                    didTryDownload = true
                    if book.storageKey != nil {
                        cachedURL = await store.ensureDownloaded(book)
                        guard cachedURL != nil else { return }
                        await store.loadProgress(book)
                    }
                    isReadable = await store.loadReadableText(book)
                }
            }
        }
    }

    private var statusText: String {
        if cachedURL == nil, book.storageKey != nil {
            "Downloading \(book.title)"
        } else {
            "Opening \(book.title)"
        }
    }
}
