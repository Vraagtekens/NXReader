import SwiftUI

struct HomeView: View {
    @ObservedObject var store: LibraryStore

    private var recentBook: ReaderBook? {
        store.books.sorted { $0.lastOpenedAt > $1.lastOpenedAt }.first
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Text("Home")
                    .font(.largeTitle.bold())
                    .padding(.top, 18)

                if let recentBook {
                    NavigationLink {
                        BookOpenView(book: recentBook, store: store)
                    } label: {
                        RecentBookCard(book: recentBook)
                    }
                    .buttonStyle(.plain)
                } else {
                    ContentUnavailableView(
                        "No Books Yet",
                        systemImage: "books.vertical",
                        description: Text("Add an EPUB from the Library tab.")
                    )
                    .frame(maxWidth: .infinity)
                }
            }
            .padding(.horizontal, 20)
        }
        .scrollIndicators(.hidden)
    }
}

struct RecentBookCard: View {
    @ObservedObject var book: ReaderBook

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack(spacing: 18) {
                BookCover(book: book, width: 116, height: 170)

                VStack(alignment: .leading, spacing: 10) {
                    Text("Latest Book")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(.secondary)
                    Text(book.title)
                        .font(.title2.bold())
                        .foregroundStyle(.primary)
                    Text(book.author)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
            }

            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(progressLabel)
                        .font(.title3.weight(.bold))
                    Spacer()
                    Text("Page \(page) / \(pageCount)")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(.secondary)
                }

                ProgressView(value: Double(page), total: Double(pageCount))
                    .tint(.primary)
            }
        }
        .padding(18)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }

    private var pageCount: Int {
        max(1, book.pageCount)
    }

    private var page: Int {
        min(max(1, book.currentPage), pageCount)
    }

    private var progressLabel: String {
        guard pageCount > 1 else {
            return "0%"
        }
        let percent = Int((Double(page - 1) / Double(pageCount - 1) * 100).rounded())
        return "\(percent)%"
    }
}

struct SmallBookCard: View {
    let book: ReaderBook
    var onRemove: (() -> Void)?
    var onRename: (() -> Void)?
    var onDownload: (() -> Void)?
    var onMarkFinished: (() -> Void)?

    var body: some View {
        VStack(alignment: .leading, spacing: 7) {
            BookCover(book: book, width: 96, height: 142)

            HStack(spacing: 8) {
                Text(progressLabel)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.secondary)

                Spacer(minLength: 4)

                if !book.isDownloaded {
                    Image(systemName: "icloud.and.arrow.down")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                }

                if hasActions {
                    Menu {
                        Button {
                            onRename?()
                        } label: {
                            Label("Rename", systemImage: "pencil")
                        }

                        Button {
                            onDownload?()
                        } label: {
                            Label("Download", systemImage: "icloud.and.arrow.down")
                        }
                        .disabled(book.isDownloaded)

                        Button {
                            onMarkFinished?()
                        } label: {
                            Label("Mark as Finished", systemImage: "checkmark.circle")
                        }

                        Button(role: .destructive) {
                            onRemove?()
                        } label: {
                            Label("Remove", systemImage: "trash")
                        }
                    } label: {
                        Image(systemName: "ellipsis")
                            .font(.caption.weight(.bold))
                            .frame(width: 24, height: 24)
                            .background(.regularMaterial, in: Circle())
                    }
                    .buttonStyle(.plain)
                }
            }
            .frame(width: 96)
        }
        .frame(width: 104, alignment: .leading)
    }

    private var progressLabel: String {
        let pageCount = max(1, book.pageCount)
        let page = min(max(1, book.currentPage), pageCount)
        guard pageCount > 1 else {
            return "0%"
        }
        let percent = Int((Double(page - 1) / Double(pageCount - 1) * 100).rounded())
        return "\(percent)%"
    }

    private var hasActions: Bool {
        onRemove != nil || onRename != nil || onDownload != nil || onMarkFinished != nil
    }
}
