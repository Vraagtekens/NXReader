import SwiftUI
import UniformTypeIdentifiers

struct LibraryView: View {
    @ObservedObject var store: LibraryStore
    @State private var isImporterPresented = false

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
                        Image(systemName: "plus")
                            .font(.system(size: 18, weight: .bold))
                            .frame(width: 38, height: 38)
                            .background(.regularMaterial, in: Circle())
                    }
                    .buttonStyle(.plain)
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
                            SmallBookCard(book: book)
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
    }

    private func importBook(_ result: Result<URL, Error>) {
        guard case .success(let url) = result else { return }
        let didAccess = url.startAccessingSecurityScopedResource()
        defer {
            if didAccess {
                url.stopAccessingSecurityScopedResource()
            }
        }

        store.addImportedBook(fileName: url.lastPathComponent)
    }
}

extension UTType {
    static let epub = UTType(filenameExtension: "epub") ?? .data
}
