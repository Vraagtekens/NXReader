import SwiftUI

struct HomeView: View {
    @ObservedObject var store: LibraryStore
    let books: [ReaderBook]
    let readingDays: [ReadingDay]

    private var recentBook: ReaderBook? {
        books.first
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                Text("Home")
                    .font(.largeTitle.bold())
                    .padding(.top, 18)

                if let recentBook {
                    NavigationLink {
                        ReaderView(book: recentBook, store: store)
                    } label: {
                        RecentBookCard(book: recentBook)
                    }
                    .buttonStyle(.plain)
                }

                VStack(alignment: .leading, spacing: 14) {
                    HStack {
                        Text("Reading Month")
                            .font(.title3.bold())
                        Spacer()
                        Text(monthTotal)
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(.secondary)
                    }

                    ReadingHeatmap(days: readingDays)
                }

                VStack(alignment: .leading, spacing: 14) {
                    Text("Continue")
                        .font(.title3.bold())

                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 14) {
                            ForEach(books.dropFirst()) { book in
                                NavigationLink {
                                    ReaderView(book: book, store: store)
                                } label: {
                                    SmallBookCard(book: book)
                                }
                                .buttonStyle(.plain)
                            }
                        }
                    }
                }
            }
            .padding(.horizontal, 20)
        }
        .scrollIndicators(.hidden)
    }

    private var monthTotal: String {
        let total = readingDays.reduce(0) { $0 + $1.minutesRead }
        let hours = Double(total) / 60.0
        return String(format: "%.1fh", hours)
    }
}

struct RecentBookCard: View {
    let book: ReaderBook

    var body: some View {
        HStack(spacing: 18) {
            BookCover(book: book, width: 108, height: 158)

            VStack(alignment: .leading, spacing: 10) {
                Text("Most Recent")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(.secondary)
                Text(book.title)
                    .font(.title2.bold())
                    .foregroundStyle(.primary)
                Text(book.author)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)

                ProgressView(value: Double(book.currentPage), total: Double(book.pageCount))
                    .tint(.primary)
                    .padding(.top, 6)

                Text("Page \(book.currentPage) of \(book.pageCount)")
                    .font(.footnote.weight(.medium))
                    .foregroundStyle(.secondary)
            }
        }
        .padding(18)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }
}

struct SmallBookCard: View {
    let book: ReaderBook

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            BookCover(book: book, width: 96, height: 142)
            Text(book.title)
                .font(.subheadline.bold())
                .lineLimit(2)
                .frame(width: 104, alignment: .leading)
            Text("\(book.currentPage)/\(book.pageCount)")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }
}

struct ReadingHeatmap: View {
    let days: [ReadingDay]

    private let columns = Array(repeating: GridItem(.flexible(), spacing: 5), count: 7)

    var body: some View {
        LazyVGrid(columns: columns, spacing: 5) {
            ForEach(days.suffix(35), id: \.date) { day in
                RoundedRectangle(cornerRadius: 3)
                    .fill(color(for: day.minutesRead))
                    .aspectRatio(1, contentMode: .fit)
                    .accessibilityLabel("\(day.minutesRead) minutes")
            }
        }
        .padding(14)
        .background(.background, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }

    private func color(for minutes: Int) -> Color {
        switch minutes {
        case 0: Color(.systemGray5)
        case 1..<15: Color.green.opacity(0.28)
        case 15..<30: Color.green.opacity(0.48)
        case 30..<60: Color.green.opacity(0.72)
        default: Color.green
        }
    }
}
