import SwiftUI

struct NotesView: View {
    let notes: [ReaderNote]

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("Notes")
                    .font(.largeTitle.bold())
                    .padding(.top, 18)

                ForEach(notes) { note in
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Text(note.bookTitle)
                                .font(.headline)
                            Spacer()
                            Text("p. \(note.page)")
                                .font(.caption.weight(.semibold))
                                .foregroundStyle(.secondary)
                        }

                        Text(note.selectedText)
                            .font(.body.weight(.medium))
                            .foregroundStyle(.primary)

                        if !note.note.isEmpty {
                            Text(note.note)
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }
                    }
                    .padding(16)
                    .background(.background, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
                }
            }
            .padding(.horizontal, 20)
        }
        .scrollIndicators(.hidden)
    }
}
