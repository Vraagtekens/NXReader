import SwiftUI
import UIKit

struct BookCover: View {
    @ObservedObject var book: ReaderBook
    let width: CGFloat
    let height: CGFloat
    @State private var coverImage: UIImage?

    var body: some View {
        ZStack(alignment: .bottomLeading) {
            LinearGradient(
                colors: [Color(hex: book.coverColorHex), Color(hex: book.coverColorHex).opacity(0.62), .black.opacity(0.28)],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )

            if let coverImage {
                Image(uiImage: coverImage)
                    .resizable()
                    .scaledToFill()
                    .frame(width: width, height: height)
                    .clipped()
            } else {
                VStack(alignment: .leading, spacing: 7) {
                    Text(book.title)
                        .font(.headline.weight(.bold))
                        .foregroundStyle(.white)
                        .lineLimit(4)
                    Text(book.author)
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.white.opacity(0.78))
                        .lineLimit(2)
                }
                .padding(12)
            }
        }
        .frame(width: width, height: height)
        .clipShape(RoundedRectangle(cornerRadius: 7, style: .continuous))
        .shadow(color: .black.opacity(0.18), radius: 12, y: 8)
        .task(id: coverLoadID) {
            coverImage = nil
            await loadCover()
        }
    }

    private var coverLoadID: String {
        "\(book.id.uuidString)-\(book.storageKey ?? "")"
    }

    private func loadCover() async {
        guard book.storageKey != nil else {
            return
        }

        guard let data = try? await BackendClient().coverImageData(book),
              let image = UIImage(data: data) else {
            return
        }

        coverImage = image
    }
}

extension Color {
    init(hex: String) {
        let scanner = Scanner(string: hex)
        var rgb: UInt64 = 0
        scanner.scanHexInt64(&rgb)

        let red = Double((rgb >> 16) & 0xFF) / 255.0
        let green = Double((rgb >> 8) & 0xFF) / 255.0
        let blue = Double(rgb & 0xFF) / 255.0

        self.init(red: red, green: green, blue: blue)
    }
}
