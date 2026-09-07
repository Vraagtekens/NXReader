import SwiftUI
import UniformTypeIdentifiers

struct LibraryView: View {
    @ObservedObject var store: LibraryStore
    @State private var isImporterPresented = false
    @State private var bookToRename: ReaderBook?
    @State private var renameTitle = ""

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                HStack {
                    Text("Library")
                        .font(.largeTitle.bold())
                    Spacer()
                    Button {
                        Task {
                            await store.refreshFromBackend()
                        }
                    } label: {
                        Image(systemName: "arrow.clockwise")
                            .font(.system(size: 17, weight: .bold))
                            .frame(width: 38, height: 38)
                            .background(.regularMaterial, in: Circle())
                    }
                    .buttonStyle(.plain)
                    Button {
                        isImporterPresented = true
                    } label: {
                        if store.uploadingBook {
                            ProgressView()
                                .frame(width: 38, height: 38)
                                .background(.regularMaterial, in: Circle())
                        } else {
                            Image(systemName: "plus")
                                .font(.system(size: 18, weight: .bold))
                                .frame(width: 38, height: 38)
                                .background(.regularMaterial, in: Circle())
                        }
                    }
                    .buttonStyle(.plain)
                    .disabled(store.uploadingBook)
                }
                .padding(.top, 18)

                if let syncError = store.syncError {
                    Text(syncError)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .padding(12)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                }

                LazyVGrid(columns: [GridItem(.adaptive(minimum: 104), spacing: 18)], alignment: .leading, spacing: 22) {
                    ForEach(store.books) { book in
                        NavigationLink {
                            BookOpenView(book: book, store: store)
                        } label: {
                            SmallBookCard(
                                book: book,
                                onRemove: {
                                    Task {
                                        await store.removeBook(book)
                                    }
                                },
                                onRename: {
                                    renameTitle = book.title
                                    bookToRename = book
                                },
                                onDownload: {
                                    Task {
                                        _ = await store.ensureDownloaded(book)
                                    }
                                },
                                onMarkFinished: {
                                    store.markFinished(book)
                                }
                            )
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
            .padding(.horizontal, 20)
        }
        .scrollIndicators(.hidden)
        .fileImporter(isPresented: $isImporterPresented, allowedContentTypes: [.epub]) { result in
            importBook(result)
        }
        .alert("Rename Book", isPresented: renameAlertBinding) {
            TextField("Title", text: $renameTitle)
            Button("Cancel", role: .cancel) {
                bookToRename = nil
            }
            Button("Rename") {
                if let bookToRename {
                    store.renameBook(bookToRename, title: renameTitle)
                }
                bookToRename = nil
            }
        }
    }

    private var renameAlertBinding: Binding<Bool> {
        Binding(
            get: { bookToRename != nil },
            set: { isPresented in
                if !isPresented {
                    bookToRename = nil
                }
            }
        )
    }

    private func importBook(_ result: Result<URL, Error>) {
        guard case .success(let url) = result else { return }

        Task {
            do {
                let localURL = try copyToTemporaryImportURL(url)
                await store.uploadImportedBook(fileURL: localURL)
            } catch {
                await MainActor.run {
                    store.syncError = error.localizedDescription
                }
            }
        }
    }

    private func copyToTemporaryImportURL(_ url: URL) throws -> URL {
        let didAccess = url.startAccessingSecurityScopedResource()
        defer {
            if didAccess {
                url.stopAccessingSecurityScopedResource()
            }
        }

        let importsDirectory = FileManager.default
            .temporaryDirectory
            .appendingPathComponent("NXReaderImports", isDirectory: true)
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: importsDirectory, withIntermediateDirectories: true)

        let destination = importsDirectory
            .appendingPathComponent(url.lastPathComponent, isDirectory: false)

        try FileManager.default.copyItem(at: url, to: destination)
        return destination
    }
}

extension UTType {
    static let epub = UTType(filenameExtension: "epub") ?? .data
}
