import Combine
import SwiftUI
import UIKit

struct ReaderView: View {
    @Environment(\.dismiss) private var dismiss
    @ObservedObject var book: ReaderBook
    @ObservedObject var store: LibraryStore
    @StateObject private var selection = ReaderSelectionState()
    @State private var isChromeVisible = true
    @State private var fontSize: CGFloat = 24
    @State private var sliceIndex = 0
    @State private var isLookupPresented = false
    @State private var isNotePromptPresented = false
    @State private var noteDraft = ""

    private var slices: [ReaderPageSlice] {
        EPUBTextPaginator.slices(from: book.sampleText, fontSize: fontSize)
    }

    private var currentSlice: ReaderPageSlice {
        slices[safe: sliceIndex] ?? ReaderPageSlice(realPage: 1, part: 1, partCount: 1, text: book.sampleText)
    }

    var body: some View {
        GeometryReader { proxy in
            ZStack {
                Color(.systemBackground)
                    .ignoresSafeArea()

                TabView(selection: $sliceIndex) {
                    ForEach(Array(slices.enumerated()), id: \.offset) { index, slice in
                        ReaderPageSurface(
                            slice: slice,
                            fontSize: fontSize,
                            highlights: book.highlights,
                            selection: selection
                        )
                        .frame(width: proxy.size.width, height: proxy.size.height)
                        .background(Color(.systemBackground))
                        .tag(index)
                    }
                }
                .tabViewStyle(.page(indexDisplayMode: .never))

                VStack {
                    ReaderTopBar(
                        book: book,
                        slice: currentSlice,
                        fontSize: $fontSize,
                        onBack: { dismiss() }
                    )
                    Spacer()
                    ReaderSelectionBar(
                        selectedText: selection.selectedText,
                        onHighlight: highlightSelection,
                        onNote: { isNotePromptPresented = true },
                        onLookup: { isLookupPresented = true }
                    )
                    ReaderBottomBar(
                        sliceIndex: $sliceIndex,
                        totalSlices: max(1, slices.count),
                        slice: currentSlice
                    )
                }
                .opacity(isChromeVisible ? 1 : 0.08)
            }
            .contentShape(Rectangle())
            .simultaneousGesture(
                TapGesture(count: 2).onEnded {
                    withAnimation(.easeInOut(duration: 0.2)) {
                        isChromeVisible.toggle()
                    }
                }
            )
        }
        .navigationBarBackButtonHidden(true)
        .toolbar(.hidden, for: .navigationBar)
        .onAppear {
            syncBookProgress(from: currentSlice)
            sliceIndex = indexForSavedProgress()
        }
        .onChange(of: sliceIndex) { _, _ in
            syncBookProgress(from: currentSlice)
        }
        .onChange(of: fontSize) { _, _ in
            sliceIndex = min(indexForSavedProgress(), max(0, slices.count - 1))
        }
        .sheet(isPresented: $isLookupPresented) {
            LookupView(term: selection.selectedText)
        }
        .alert("Add Note", isPresented: $isNotePromptPresented) {
            TextField("Note", text: $noteDraft)
            Button("Save") {
                addNote()
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text(selection.selectedText.isEmpty ? "Select text first." : selection.selectedText)
        }
    }

    private func indexForSavedProgress() -> Int {
        slices.firstIndex {
            $0.realPage == book.currentPage && $0.part == book.currentPart
        } ?? 0
    }

    private func syncBookProgress(from slice: ReaderPageSlice) {
        book.currentPage = slice.realPage
        book.currentPart = slice.part
        book.pageCount = slices.map(\.realPage).max() ?? 1
        book.lastOpenedAt = .now
    }

    private func highlightSelection() {
        let selected = selection.selectedText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !selected.isEmpty, !book.highlights.contains(selected) else { return }
        book.highlights.append(selected)
    }

    private func addNote() {
        let selected = selection.selectedText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !selected.isEmpty else { return }
        store.addNote(
            book: book,
            page: currentSlice.realPage,
            selectedText: selected,
            note: noteDraft.trimmingCharacters(in: .whitespacesAndNewlines)
        )
        noteDraft = ""
    }
}

struct ReaderTopBar: View {
    let book: ReaderBook
    let slice: ReaderPageSlice
    @Binding var fontSize: CGFloat
    let onBack: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            Button(action: onBack) {
                Image(systemName: "chevron.left")
                    .font(.system(size: 18, weight: .bold))
                    .frame(width: 36, height: 36)
                    .background(.regularMaterial, in: Circle())
            }
            .buttonStyle(.plain)

            VStack(alignment: .leading, spacing: 2) {
                Text(book.title)
                    .font(.headline)
                    .lineLimit(1)
                Text(slice.pageLabel)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Button {
                fontSize = max(18, fontSize - 2)
            } label: {
                Image(systemName: "textformat.size.smaller")
            }
            .buttonStyle(.plain)

            Button {
                fontSize = min(34, fontSize + 2)
            } label: {
                Image(systemName: "textformat.size.larger")
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 10)
        .background(.ultraThinMaterial)
    }
}

struct ReaderSelectionBar: View {
    let selectedText: String
    let onHighlight: () -> Void
    let onNote: () -> Void
    let onLookup: () -> Void

    private var hasSelection: Bool {
        !selectedText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        if hasSelection {
            HStack(spacing: 10) {
                Text(selectedText)
                    .font(.caption)
                    .lineLimit(1)
                    .foregroundStyle(.secondary)

                Spacer()

                Button("Highlight", action: onHighlight)
                Button("Note", action: onNote)
                Button("Look Up", action: onLookup)
            }
            .font(.caption.weight(.semibold))
            .buttonStyle(.bordered)
            .padding(.horizontal, 14)
            .padding(.vertical, 10)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            .padding(.horizontal, 14)
        }
    }
}

struct ReaderBottomBar: View {
    @Binding var sliceIndex: Int
    let totalSlices: Int
    let slice: ReaderPageSlice

    var body: some View {
        VStack(spacing: 8) {
            Slider(value: sliceBinding, in: 0...Double(max(0, totalSlices - 1)), step: 1)
                .tint(.primary)
            Text(slice.pageLabel)
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)
        }
        .padding(.horizontal, 20)
        .padding(.top, 12)
        .padding(.bottom, 20)
        .background(.ultraThinMaterial)
    }

    private var sliceBinding: Binding<Double> {
        Binding {
            Double(sliceIndex)
        } set: { value in
            sliceIndex = Int(value)
        }
    }
}

struct ReaderPageSurface: View {
    let slice: ReaderPageSlice
    let fontSize: CGFloat
    let highlights: [String]
    @ObservedObject var selection: ReaderSelectionState

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            SelectableReaderText(
                text: slice.text,
                fontSize: fontSize,
                highlights: highlights,
                selectedText: $selection.selectedText
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .padding(.horizontal, 26)
            .padding(.top, 70)
            .padding(.bottom, 92)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .clipped()
    }
}

struct SelectableReaderText: UIViewRepresentable {
    let text: String
    let fontSize: CGFloat
    let highlights: [String]
    @Binding var selectedText: String

    func makeUIView(context: Context) -> UITextView {
        let textView = UITextView()
        textView.delegate = context.coordinator
        textView.isEditable = false
        textView.isSelectable = true
        textView.isScrollEnabled = false
        textView.backgroundColor = .clear
        textView.textContainerInset = .zero
        textView.textContainer.lineFragmentPadding = 0
        textView.setContentCompressionResistancePriority(.required, for: .vertical)
        return textView
    }

    func updateUIView(_ textView: UITextView, context: Context) {
        context.coordinator.parent = self
        textView.attributedText = attributedText()
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    private func attributedText() -> NSAttributedString {
        let paragraph = NSMutableParagraphStyle()
        paragraph.lineSpacing = 8
        paragraph.paragraphSpacing = 15

        let attributed = NSMutableAttributedString(
            string: text,
            attributes: [
                .font: UIFont(name: "Georgia", size: fontSize) ?? UIFont.systemFont(ofSize: fontSize),
                .foregroundColor: UIColor.label,
                .paragraphStyle: paragraph
            ]
        )

        for highlight in highlights where !highlight.isEmpty {
            let nsText = text as NSString
            var searchRange = NSRange(location: 0, length: nsText.length)
            while true {
                let found = nsText.range(of: highlight, options: [.caseInsensitive], range: searchRange)
                guard found.location != NSNotFound else { break }
                attributed.addAttribute(.backgroundColor, value: UIColor.systemYellow.withAlphaComponent(0.45), range: found)
                let nextLocation = found.location + found.length
                searchRange = NSRange(location: nextLocation, length: nsText.length - nextLocation)
            }
        }

        return attributed
    }

    final class Coordinator: NSObject, UITextViewDelegate {
        var parent: SelectableReaderText

        init(parent: SelectableReaderText) {
            self.parent = parent
        }

        func textViewDidChangeSelection(_ textView: UITextView) {
            guard textView.selectedRange.length > 0,
                  let range = Range(textView.selectedRange, in: textView.text)
            else {
                parent.selectedText = ""
                return
            }

            parent.selectedText = String(textView.text[range])
        }
    }
}

final class ReaderSelectionState: ObservableObject {
    @Published var selectedText = ""
}

struct LookupView: UIViewControllerRepresentable {
    let term: String

    func makeUIViewController(context: Context) -> UIReferenceLibraryViewController {
        UIReferenceLibraryViewController(term: term)
    }

    func updateUIViewController(_ uiViewController: UIReferenceLibraryViewController, context: Context) {}
}

struct ReaderPageSlice: Identifiable {
    let id = UUID()
    let realPage: Int
    let part: Int
    let partCount: Int
    let text: String

    var pageLabel: String {
        partCount > 1 ? "Page \(realPage) · \(part)/\(partCount)" : "Page \(realPage)"
    }
}

enum EPUBTextPaginator {
    static func slices(from text: String, fontSize: CGFloat) -> [ReaderPageSlice] {
        let realPageTarget = 1_850
        let partTarget = max(420, Int(930 - ((fontSize - 24) * 28)))
        let realPages = chunk(text: text, targetCharacters: realPageTarget)

        return realPages.enumerated().flatMap { realIndex, realText in
            let parts = chunk(text: realText, targetCharacters: partTarget)
            return parts.enumerated().map { partIndex, partText in
                ReaderPageSlice(
                    realPage: realIndex + 1,
                    part: partIndex + 1,
                    partCount: parts.count,
                    text: partText
                )
            }
        }
    }

    private static func chunk(text: String, targetCharacters: Int) -> [String] {
        let paragraphs = text.components(separatedBy: "\n\n")
        var chunks: [String] = []
        var current = ""

        for paragraph in paragraphs {
            let candidate = current.isEmpty ? paragraph : current + "\n\n" + paragraph
            if candidate.count > targetCharacters, !current.isEmpty {
                chunks.append(current)
                current = paragraph
            } else if paragraph.count > targetCharacters {
                if !current.isEmpty {
                    chunks.append(current)
                    current = ""
                }
                chunks.append(contentsOf: splitLongParagraph(paragraph, targetCharacters: targetCharacters))
            } else {
                current = candidate
            }
        }

        if !current.isEmpty {
            chunks.append(current)
        }

        return chunks.isEmpty ? [text] : chunks
    }

    private static func splitLongParagraph(_ paragraph: String, targetCharacters: Int) -> [String] {
        var words = paragraph.split(separator: " ")
        var chunks: [String] = []
        var current = ""

        while !words.isEmpty {
            let word = words.removeFirst()
            let candidate = current.isEmpty ? String(word) : current + " " + word
            if candidate.count > targetCharacters, !current.isEmpty {
                chunks.append(current)
                current = String(word)
            } else {
                current = candidate
            }
        }

        if !current.isEmpty {
            chunks.append(current)
        }

        return chunks
    }
}

private extension Array {
    subscript(safe index: Int) -> Element? {
        indices.contains(index) ? self[index] : nil
    }
}
