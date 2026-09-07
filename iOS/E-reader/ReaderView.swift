import SwiftUI
import UIKit

struct ReaderView: View {
    @Environment(\.dismiss) private var dismiss
    @ObservedObject var book: ReaderBook
    @ObservedObject var store: LibraryStore
    @AppStorage("reader.fontSize") private var storedFontSize = 22.0
    @AppStorage("reader.fontFamily") private var storedFontFamily = ReaderFont.georgia.rawValue
    @AppStorage("reader.darkMode") private var useDarkMode = true
    @AppStorage("reader.rotationLocked") private var lockRotation = false
    @State private var pageIndex = 0
    @State private var showChrome = true
    @State private var selectedText = ""
    @State private var activeSheet: ReaderSheet?
    @State private var isMenuExpanded = false
    @State private var renderedPages: [ReaderPage] = []
    @State private var readerSize: CGSize = .zero
    @State private var paginationTask: Task<Void, Never>?
    @State private var progressSaveTask: Task<Void, Never>?
    @State private var liveFontSize = 22.0
    @State private var noteDraft = ""

    private var fontSize: CGFloat {
        CGFloat(liveFontSize)
    }

    private var fontFamily: ReaderFont {
        ReaderFont(rawValue: storedFontFamily) ?? .georgia
    }

    private var pages: [ReaderPage] {
        renderedPages.isEmpty ? [.empty] : renderedPages
    }

    private var chapters: [ReaderChapter] {
        ReaderChapter.chapters(from: pages)
    }

    private var theme: ReaderTheme {
        useDarkMode ? .dark : .light
    }

    private var textAreaSize: CGSize {
        let screen = UIScreen.main.bounds
        let width = readerSize.width > 100 ? readerSize.width : screen.width
        let height = readerSize.height > 500 ? readerSize.height : screen.height
        return CGSize(
            width: max(1, width - (ReaderLayout.horizontalPadding * 2)),
            height: max(1, height - ReaderLayout.topPadding - ReaderLayout.bottomPadding)
        )
    }

    var body: some View {
        ZStack {
            theme.background.ignoresSafeArea()

            TabView(selection: $pageIndex) {
                ForEach(Array(pages.enumerated()), id: \.offset) { index, page in
                    ReaderPageView(
                        page: page,
                        fontSize: fontSize,
                        fontFamily: fontFamily,
                        theme: theme,
                        images: book.inlineImages,
                        highlights: book.highlights,
                        selectedText: $selectedText
                    )
                    .tag(index + 1)
                    .contentShape(Rectangle())
                    .onTapGesture {
                        withAnimation(.easeInOut(duration: 0.18)) {
                            showChrome.toggle()
                        }
                    }
                }
            }
            .tabViewStyle(.page(indexDisplayMode: .never))
            .ignoresSafeArea()

            if showChrome {
                ReaderChrome(
                    title: book.title,
                    pageLabel: pageLabel,
                    selectedText: selectedText,
                    isMenuExpanded: $isMenuExpanded,
                    lockRotation: lockRotation,
                    onBack: { dismiss() },
                    onHighlight: highlightSelection,
                    onNote: {
                        noteDraft = ""
                        activeSheet = .note
                    },
                    onContents: { activeSheet = .contents },
                    onSettings: { activeSheet = .settings },
                    onToggleRotationLock: { lockRotation.toggle() }
                )
                .transition(.opacity)
            }
        }
        .background {
            GeometryReader { proxy in
                Color.clear.preference(key: ReaderSizePreferenceKey.self, value: proxy.size)
            }
        }
        .onPreferenceChange(ReaderSizePreferenceKey.self) { size in
            guard abs(size.width - readerSize.width) > 1 || abs(size.height - readerSize.height) > 1 else {
                return
            }
            readerSize = size
            rebuildPages(targetPageIndex: max(1, pageIndex))
        }
        .preferredColorScheme(useDarkMode ? .dark : .light)
        .navigationBarBackButtonHidden(true)
        .toolbar(.hidden, for: .navigationBar)
        .toolbar(.hidden, for: .tabBar)
        .onAppear {
            liveFontSize = storedFontSize
            store.markOpened(book)
            pageIndex = max(1, book.currentPage)
            rebuildPages(targetPageIndex: max(1, book.currentPage))
            OrientationController.shared.setRotationLocked(lockRotation)
        }
        .onChange(of: pageIndex) { _, newValue in
            selectedText = ""
            book.currentPage = min(max(1, newValue), pages.count)
            book.pageCount = max(1, pages.count)
            scheduleProgressSave()
        }
        .onChange(of: storedFontSize) { _, _ in
            liveFontSize = storedFontSize
            rebuildPages(targetPageIndex: pageIndex)
        }
        .onChange(of: storedFontFamily) { _, _ in
            rebuildPages(targetPageIndex: pageIndex)
        }
        .onChange(of: book.sampleText) { _, _ in
            rebuildPages(targetPageIndex: pageIndex)
        }
        .onChange(of: lockRotation) { _, newValue in
            OrientationController.shared.setRotationLocked(newValue)
        }
        .onDisappear {
            paginationTask?.cancel()
            progressSaveTask?.cancel()
            Task {
                await store.saveProgress(book)
            }
            OrientationController.shared.setRotationLocked(false)
        }
        .sheet(item: $activeSheet) { sheet in
            switch sheet {
            case .contents:
                ReaderContentsSheet(chapters: chapters, pageIndex: $pageIndex)
                    .presentationDetents([.medium, .large])
                    .presentationDragIndicator(.visible)
            case .settings:
                ReaderSettingsSheet(
                    liveFontSize: $liveFontSize,
                    storedFontSize: $storedFontSize,
                    storedFontFamily: $storedFontFamily,
                    useDarkMode: $useDarkMode
                )
                .presentationDetents([.medium])
                .presentationDragIndicator(.visible)
            case .note:
                ReaderNoteSheet(
                    selectedText: selectedText,
                    note: $noteDraft,
                    onSave: saveNote
                )
                .presentationDetents([.medium])
                .presentationDragIndicator(.visible)
            }
        }
    }

    private func highlightSelection() {
        let text = selectedText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty, !book.highlights.contains(text) else {
            return
        }
        book.highlights.append(text)
        selectedText = ""
    }

    private func saveNote() {
        store.addNote(
            book: book,
            page: max(1, pageIndex),
            selectedText: selectedText,
            note: noteDraft
        )
        noteDraft = ""
        selectedText = ""
        activeSheet = nil
    }

    private func rebuildPages(targetPageIndex: Int? = nil) {
        paginationTask?.cancel()

        let text = book.sampleText
        let size = CGFloat(storedFontSize)
        let area = textAreaSize
        let destinationIndex = max(1, targetPageIndex ?? pageIndex)

        paginationTask = Task {
            let builtPages = await Task.detached(priority: .userInitiated) {
                ReaderFastPaginator.pages(
                    from: text,
                    fontSize: size,
                    pageSize: area
                )
            }.value

            guard !Task.isCancelled else {
                return
            }

            renderedPages = builtPages
            pageIndex = min(destinationIndex, max(1, builtPages.count))
            book.pageCount = max(1, builtPages.count)
            book.currentPage = min(max(1, pageIndex), book.pageCount)
            scheduleProgressSave()
        }
    }

    private func scheduleProgressSave() {
        progressSaveTask?.cancel()
        progressSaveTask = Task {
            try? await Task.sleep(nanoseconds: 350_000_000)
            guard !Task.isCancelled else {
                return
            }
            await store.saveProgress(book)
        }
    }

    private var pageLabel: String {
        return "Page \(pageIndex) of \(max(1, pages.count))"
    }
}

private struct ReaderSizePreferenceKey: PreferenceKey {
    static var defaultValue: CGSize = .zero

    static func reduce(value: inout CGSize, nextValue: () -> CGSize) {
        value = nextValue()
    }
}

private enum ReaderSheet: String, Identifiable {
    case contents
    case settings
    case note

    var id: String { rawValue }
}

private enum ReaderLayout {
    static let horizontalPadding: CGFloat = 32
    static let topPadding: CGFloat = 96
    static let bottomPadding: CGFloat = 88
}

private struct ReaderChrome: View {
    let title: String
    let pageLabel: String
    let selectedText: String
    @Binding var isMenuExpanded: Bool
    let lockRotation: Bool
    let onBack: () -> Void
    let onHighlight: () -> Void
    let onNote: () -> Void
    let onContents: () -> Void
    let onSettings: () -> Void
    let onToggleRotationLock: () -> Void

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                Button(action: onBack) {
                    Image(systemName: "chevron.left")
                        .font(.system(size: 18, weight: .semibold))
                        .frame(width: 38, height: 38)
                        .background(.regularMaterial, in: Circle())
                }
                .buttonStyle(.plain)

                Text(title)
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .frame(maxWidth: .infinity)

                Color.clear.frame(width: 38, height: 38)
            }
            .padding(.horizontal, 16)
            .padding(.top, 12)

            if !selectedText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                HStack(spacing: 10) {
                    Button(action: onHighlight) {
                        Label("Highlight", systemImage: "highlighter")
                            .font(.caption.weight(.semibold))
                            .padding(.horizontal, 14)
                            .padding(.vertical, 9)
                            .background(.regularMaterial, in: Capsule())
                    }
                    .buttonStyle(.plain)

                    Button(action: onNote) {
                        Label("Note", systemImage: "square.and.pencil")
                            .font(.caption.weight(.semibold))
                            .padding(.horizontal, 14)
                            .padding(.vertical, 9)
                            .background(.regularMaterial, in: Capsule())
                    }
                    .buttonStyle(.plain)
                }
                .padding(.top, 8)
            }

            Spacer()

            HStack(alignment: .bottom) {
                Spacer()

                Text(pageLabel)
                    .font(.caption.weight(.medium))
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 7)
                    .background(.regularMaterial, in: Capsule())

                Spacer()

                ReaderCornerMenu(
                    isExpanded: $isMenuExpanded,
                    lockRotation: lockRotation,
                    onContents: onContents,
                    onSettings: onSettings,
                    onToggleRotationLock: onToggleRotationLock
                )
            }
            .padding(.horizontal, 18)
            .padding(.bottom, 14)
        }
    }
}

private struct ReaderNoteSheet: View {
    let selectedText: String
    @Binding var note: String
    let onSave: () -> Void
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            Form {
                Section("Selection") {
                    Text(selectedText)
                        .font(.callout)
                        .foregroundStyle(.secondary)
                }

                Section("Note") {
                    TextEditor(text: $note)
                        .frame(minHeight: 120)
                }
            }
            .navigationTitle("Add Note")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        dismiss()
                    }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        onSave()
                    }
                    .disabled(note.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
    }
}

private struct ReaderCornerMenu: View {
    @Binding var isExpanded: Bool
    let lockRotation: Bool
    let onContents: () -> Void
    let onSettings: () -> Void
    let onToggleRotationLock: () -> Void

    var body: some View {
        VStack(spacing: 10) {
            if isExpanded {
                ReaderMenuButton(icon: "list.bullet", action: {
                    isExpanded = false
                    onContents()
                })
                ReaderMenuButton(icon: "textformat", action: {
                    isExpanded = false
                    onSettings()
                })
                ReaderMenuButton(icon: "lock.rotation", isActive: lockRotation, action: {
                    onToggleRotationLock()
                })
            }

            ReaderMenuButton(icon: isExpanded ? "xmark" : "ellipsis", isActive: false, action: {
                withAnimation(.spring(response: 0.25, dampingFraction: 0.82)) {
                    isExpanded.toggle()
                }
            })
        }
    }
}

private struct ReaderMenuButton: View {
    let icon: String
    var isActive = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: icon)
                .font(.system(size: 17, weight: .semibold))
                .frame(width: 40, height: 40)
                .foregroundStyle(isActive ? .white : .primary)
                .background(isActive ? Color.accentColor : Color.clear, in: Circle())
                .background(.regularMaterial, in: Circle())
        }
        .buttonStyle(.plain)
    }
}

private struct ReaderCoverPage: View {
    @ObservedObject var book: ReaderBook
    let theme: ReaderTheme

    var body: some View {
        GeometryReader { proxy in
            VStack(spacing: 20) {
                Spacer(minLength: 40)

                BookCover(
                    book: book,
                    width: min(proxy.size.width * 0.58, 260),
                    height: min(proxy.size.width * 0.58, 260) * 1.46
                )

                VStack(spacing: 7) {
                    Text(book.title)
                        .font(.title3.weight(.semibold))
                        .multilineTextAlignment(.center)
                        .foregroundStyle(Color(theme.textUIColor))
                    Text(book.author)
                        .font(.subheadline)
                        .multilineTextAlignment(.center)
                        .foregroundStyle(.secondary)
                }
                .padding(.horizontal, 32)

                Spacer(minLength: 60)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}

private struct ReaderPageView: View {
    let page: ReaderPage
    let fontSize: CGFloat
    let fontFamily: ReaderFont
    let theme: ReaderTheme
    let images: [String: ReaderInlineImage]
    let highlights: [String]
    @Binding var selectedText: String

    var body: some View {
        GeometryReader { proxy in
            SelectableReaderText(
                blocks: page.blocks,
                fontSize: fontSize,
                fontFamily: fontFamily,
                theme: theme,
                images: images,
                highlights: highlights,
                selectedText: $selectedText
            )
            .frame(width: proxy.size.width, height: proxy.size.height, alignment: .topLeading)
            .clipped()
        }
        .padding(.horizontal, ReaderLayout.horizontalPadding)
        .padding(.top, ReaderLayout.topPadding)
        .padding(.bottom, ReaderLayout.bottomPadding)
        .clipped()
    }
}

private struct SelectableReaderText: UIViewRepresentable {
    let blocks: [ReaderTextBlock]
    let fontSize: CGFloat
    let fontFamily: ReaderFont
    let theme: ReaderTheme
    let images: [String: ReaderInlineImage]
    let highlights: [String]
    @Binding var selectedText: String

    func makeUIView(context: Context) -> UITextView {
        let textView = ReaderLockedTextView()
        textView.delegate = context.coordinator
        textView.isEditable = false
        textView.isSelectable = true
        textView.isScrollEnabled = false
        textView.backgroundColor = .clear
        textView.textContainerInset = .zero
        textView.textContainer.lineFragmentPadding = 0
        textView.textContainer.widthTracksTextView = true
        textView.textContainer.lineBreakMode = .byWordWrapping
        textView.contentInset = .zero
        textView.scrollIndicatorInsets = .zero
        textView.contentInsetAdjustmentBehavior = .never
        textView.showsVerticalScrollIndicator = false
        textView.adjustsFontForContentSizeCategory = false
        textView.clipsToBounds = true
        textView.layer.masksToBounds = true
        textView.setContentCompressionResistancePriority(.required, for: .vertical)
        textView.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        return textView
    }

    func updateUIView(_ textView: UITextView, context: Context) {
        context.coordinator.parent = self
        let signature = renderSignature
        if context.coordinator.renderSignature != signature {
            context.coordinator.renderSignature = signature
            textView.attributedText = attributedText()
        }
        textView.tintColor = theme.tintUIColor
        textView.textContainer.size = CGSize(
            width: max(1, textView.bounds.width),
            height: max(1, textView.bounds.height)
        )
        textView.contentInset = .zero
        textView.setContentOffset(.zero, animated: false)
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    private func attributedText() -> NSAttributedString {
        let output = NSMutableAttributedString()

        for (index, block) in blocks.enumerated() {
            if block.kind == .image {
                output.append(imageAttachment(for: block))
            } else {
                output.append(block.attributedText(fontSize: fontSize, fontFamily: fontFamily, theme: theme))
            }
            if index < blocks.count - 1 {
                output.append(NSAttributedString(string: "\n"))
            }
        }

        applyHighlights(to: output)
        return output
    }

    private func imageAttachment(for block: ReaderTextBlock) -> NSAttributedString {
        guard let inlineImage = images[block.markdown],
              let image = UIImage(data: inlineImage.data)
        else {
            return NSAttributedString(string: "")
        }

        let maxWidth: CGFloat = 260
        let ratio = image.size.width > 0 ? image.size.height / image.size.width : 0.65
        let attachment = NSTextAttachment()
        attachment.image = image
        attachment.bounds = CGRect(x: 0, y: -4, width: maxWidth, height: maxWidth * ratio)

        let paragraph = NSMutableParagraphStyle()
        paragraph.alignment = .center
        paragraph.paragraphSpacing = 12

        let output = NSMutableAttributedString(attachment: attachment)
        output.addAttribute(.paragraphStyle, value: paragraph, range: NSRange(location: 0, length: output.length))
        return output
    }

    private var renderSignature: String {
        [
            blocks.map { "\($0.kind.signature):\($0.markdown)" }.joined(separator: "|"),
            String(Double(fontSize)),
            fontFamily.rawValue,
            theme.id,
            images.keys.sorted().joined(separator: "|"),
            highlights.joined(separator: "|")
        ].joined(separator: "::")
    }

    private func applyHighlights(to attributed: NSMutableAttributedString) {
        let fullText = attributed.string as NSString
        for highlight in highlights where !highlight.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            var range = NSRange(location: 0, length: fullText.length)
            while true {
                let found = fullText.range(of: highlight, options: [.caseInsensitive], range: range)
                guard found.location != NSNotFound else {
                    break
                }
                attributed.addAttribute(
                    .backgroundColor,
                    value: UIColor.systemYellow.withAlphaComponent(0.38),
                    range: found
                )
                let nextLocation = found.location + found.length
                range = NSRange(location: nextLocation, length: fullText.length - nextLocation)
            }
        }
    }

    final class Coordinator: NSObject, UITextViewDelegate {
        var parent: SelectableReaderText
        var renderSignature = ""

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
            textView.setContentOffset(.zero, animated: false)
            DispatchQueue.main.async {
                textView.setContentOffset(.zero, animated: false)
            }
        }
    }
}

private final class ReaderLockedTextView: UITextView {
    override func setContentOffset(_ contentOffset: CGPoint, animated: Bool) {
        super.setContentOffset(.zero, animated: false)
    }

    override func scrollRangeToVisible(_ range: NSRange) {}

    override func layoutSubviews() {
        super.layoutSubviews()
        super.setContentOffset(.zero, animated: false)
    }
}

private struct ReaderContentsSheet: View {
    let chapters: [ReaderChapter]
    @Binding var pageIndex: Int
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List(chapters) { chapter in
                Button {
                    pageIndex = chapter.pageIndex
                    dismiss()
                } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(chapter.title)
                                .font(.body)
                                .foregroundStyle(.primary)
                            Text("Page \(chapter.pageIndex)")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Image(systemName: "chevron.right")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.tertiary)
                    }
                }
                .buttonStyle(.plain)
            }
            .navigationTitle("Contents")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

private struct ReaderSettingsSheet: View {
    @Binding var liveFontSize: Double
    @Binding var storedFontSize: Double
    @Binding var storedFontFamily: String
    @Binding var useDarkMode: Bool

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack {
                        Button {
                            liveFontSize = max(16, liveFontSize - 2)
                            storedFontSize = liveFontSize
                        } label: {
                            Image(systemName: "textformat.size.smaller")
                                .frame(width: 34, height: 34)
                        }
                        .buttonStyle(.borderless)

                        Slider(
                            value: $liveFontSize,
                            in: 16...34,
                            step: 1,
                            onEditingChanged: { isEditing in
                                if !isEditing {
                                    storedFontSize = liveFontSize
                                }
                            }
                        )

                        Button {
                            liveFontSize = min(34, liveFontSize + 2)
                            storedFontSize = liveFontSize
                        } label: {
                            Image(systemName: "textformat.size.larger")
                                .frame(width: 34, height: 34)
                        }
                        .buttonStyle(.borderless)
                    }
                }

                Section {
                    Picker("Font", selection: $storedFontFamily) {
                        ForEach(ReaderFont.allCases) { font in
                            Text(font.title).tag(font.rawValue)
                        }
                    }
                }

                Section {
                    Toggle("Dark Mode", isOn: $useDarkMode)
                }
            }
            .navigationTitle("Theme & Settings")
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

private struct ReaderPage {
    let id: Int
    let text: String
    let blocks: [ReaderTextBlock]

    static let empty = ReaderPage(id: 0, text: "", blocks: [])
}

private struct ReaderTextBlock: Identifiable {
    enum Kind {
        case heading1
        case heading2
        case heading3
        case metadata
        case listItem
        case image
        case paragraph
    }

    var id: String {
        "\(kind.signature)-\(markdown)"
    }

    let kind: Kind
    let markdown: String

    var plainTitle: String {
        markdown
            .replacingOccurrences(of: "**", with: "")
            .replacingOccurrences(of: "_", with: "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }

    func attributedText(fontSize: CGFloat, fontFamily: ReaderFont, theme: ReaderTheme) -> NSAttributedString {
        let baseSize = self.baseSize(fontSize)
        let paragraph = NSMutableParagraphStyle()
        paragraph.lineSpacing = lineSpacing
        paragraph.paragraphSpacing = paragraphSpacing
        paragraph.alignment = kind.isCentered ? .center : .natural
        paragraph.lineBreakMode = .byWordWrapping
        if kind == .listItem {
            paragraph.firstLineHeadIndent = 10
            paragraph.headIndent = 28
        }

        let output = NSMutableAttributedString()
        var index = markdown.startIndex
        var buffer = ""
        var isBold = kind.isHeading
        var isItalic = false

        func flush() {
            guard !buffer.isEmpty else {
                return
            }
            output.append(
                NSAttributedString(
                    string: buffer,
                    attributes: [
                        .font: fontFamily.uiFont(size: baseSize, bold: isBold, italic: isItalic),
                        .foregroundColor: theme.textUIColor,
                        .paragraphStyle: paragraph
                    ]
                )
            )
            buffer = ""
        }

        while index < markdown.endIndex {
            if markdown[index...].hasPrefix("**") {
                flush()
                isBold.toggle()
                index = markdown.index(index, offsetBy: 2)
            } else if markdown[index] == "_" {
                flush()
                isItalic.toggle()
                index = markdown.index(after: index)
            } else {
                buffer.append(markdown[index])
                index = markdown.index(after: index)
            }
        }
        flush()
        return output
    }

    private var lineSpacing: CGFloat {
        switch kind {
        case .heading1, .heading2, .heading3:
            2
        case .metadata:
            4
        case .image:
            0
        case .listItem, .paragraph:
            7
        }
    }

    private var paragraphSpacing: CGFloat {
        switch kind {
        case .heading1:
            18
        case .heading2, .heading3:
            14
        case .metadata:
            8
        case .image:
            12
        case .listItem, .paragraph:
            5
        }
    }

    private func baseSize(_ fontSize: CGFloat) -> CGFloat {
        switch kind {
        case .heading1:
            fontSize + 13
        case .heading2:
            fontSize + 8
        case .heading3:
            fontSize + 4
        case .metadata:
            fontSize - 1
        case .image:
            fontSize
        case .listItem, .paragraph:
            fontSize
        }
    }

    static func blocks(from text: String) -> [ReaderTextBlock] {
        text
            .components(separatedBy: "\n\n")
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }
            .map { raw in
                switch inferBlock(from: raw) {
                case .heading1:
                    ReaderTextBlock(kind: .heading1, markdown: markdownBody(raw, markerLength: 2))
                case .heading2:
                    ReaderTextBlock(kind: .heading2, markdown: markdownBody(raw, markerLength: 3))
                case .heading3:
                    ReaderTextBlock(kind: .heading3, markdown: markdownBody(raw, markerLength: 4))
                case .metadata:
                    ReaderTextBlock(kind: .metadata, markdown: raw)
                case .listItem:
                    ReaderTextBlock(kind: .listItem, markdown: listBody(raw))
                case .image:
                    ReaderTextBlock(kind: .image, markdown: raw)
                case .paragraph:
                    ReaderTextBlock(kind: .paragraph, markdown: raw)
                }
            }
    }

    private static func inferBlock(from raw: String) -> Kind {
        let lowered = raw.lowercased()
        if raw.hasPrefix("# ") || lowered.contains(" h1") || lowered.hasPrefix("grand titre h1") {
            return .heading1
        }
        if raw.hasPrefix("## ") || lowered.contains(" h2") || lowered.hasPrefix("sous-titre h2") {
            return .heading2
        }
        if raw.hasPrefix("### ") || lowered.contains(" h3") || lowered.hasPrefix("titre h3") {
            return .heading3
        }
        if lowered.hasPrefix("by ")
            || lowered.hasPrefix("author:")
            || lowered.hasPrefix("writer:")
            || lowered.hasPrefix("auteur:")
            || lowered.hasPrefix("auteur ")
        {
            return .metadata
        }
        if raw.hasPrefix("- ") || lowered.hasPrefix("liste:") {
            return .listItem
        }
        if raw.hasPrefix("[[NX_IMAGE_") && raw.hasSuffix("]]") {
            return .image
        }
        return .paragraph
    }

    private static func markdownBody(_ raw: String, markerLength: Int) -> String {
        if raw.hasPrefix(String(repeating: "#", count: markerLength - 1) + " ") {
            return String(raw.dropFirst(markerLength))
        }
        return raw
    }

    private static func listBody(_ raw: String) -> String {
        if raw.hasPrefix("- ") {
            return "• " + String(raw.dropFirst(2))
        }
        if raw.lowercased().hasPrefix("liste:") {
            return "• " + raw.dropFirst("Liste:".count).trimmingCharacters(in: .whitespacesAndNewlines)
        }
        return "• " + raw
    }
}

private extension ReaderTextBlock.Kind {
    var signature: String {
        switch self {
        case .heading1: "h1"
        case .heading2: "h2"
        case .heading3: "h3"
        case .metadata: "meta"
        case .listItem: "li"
        case .image: "img"
        case .paragraph: "p"
        }
    }

    var isHeading: Bool {
        switch self {
        case .heading1, .heading2, .heading3:
            true
        case .metadata, .listItem, .image, .paragraph:
            false
        }
    }

    var isCentered: Bool {
        switch self {
        case .heading1, .heading2, .heading3, .metadata, .image:
            true
        case .listItem, .paragraph:
            false
        }
    }
}

private struct ReaderChapter: Identifiable {
    var id: Int { pageIndex }
    let title: String
    let pageIndex: Int

    static func chapters(from pages: [ReaderPage]) -> [ReaderChapter] {
        let chapters = pages.enumerated().compactMap { index, page -> ReaderChapter? in
            guard let heading = page.blocks.first(where: { $0.kind.isHeading }) else {
                return nil
            }
            return ReaderChapter(title: heading.plainTitle, pageIndex: index + 1)
        }
        return chapters.isEmpty ? [ReaderChapter(title: "Start", pageIndex: 1)] : chapters
    }
}

private enum ReaderFont: String, CaseIterable, Identifiable {
    case georgia
    case newYork
    case palatino
    case system

    var id: String { rawValue }

    var title: String {
        switch self {
        case .georgia: "Georgia"
        case .newYork: "New York"
        case .palatino: "Palatino"
        case .system: "System"
        }
    }

    func uiFont(size: CGFloat, bold: Bool, italic: Bool) -> UIFont {
        let name: String?
        switch self {
        case .georgia:
            name = georgiaName(bold: bold, italic: italic)
        case .newYork:
            name = "NewYork-Regular"
        case .palatino:
            name = palatinoName(bold: bold, italic: italic)
        case .system:
            return systemFont(size: size, bold: bold, italic: italic)
        }

        if let name, let font = UIFont(name: name, size: size) {
            return font
        }
        return systemFont(size: size, bold: bold, italic: italic)
    }

    private func georgiaName(bold: Bool, italic: Bool) -> String {
        if bold && italic { return "Georgia-BoldItalic" }
        if bold { return "Georgia-Bold" }
        if italic { return "Georgia-Italic" }
        return "Georgia"
    }

    private func palatinoName(bold: Bool, italic: Bool) -> String {
        if bold && italic { return "Palatino-BoldItalic" }
        if bold { return "Palatino-Bold" }
        if italic { return "Palatino-Italic" }
        return "Palatino-Roman"
    }

    private func systemFont(size: CGFloat, bold: Bool, italic: Bool) -> UIFont {
        var font = UIFont.systemFont(ofSize: size, weight: bold ? .semibold : .regular)
        if italic, let descriptor = font.fontDescriptor.withSymbolicTraits(.traitItalic) {
            font = UIFont(descriptor: descriptor, size: size)
        }
        return font
    }
}

private struct ReaderTheme {
    let id: String
    let background: Color
    let textUIColor: UIColor
    let tintUIColor: UIColor

    static let light = ReaderTheme(
        id: "light",
        background: Color(.systemBackground),
        textUIColor: .label,
        tintUIColor: .label
    )

    static let dark = ReaderTheme(
        id: "dark",
        background: Color(red: 0.06, green: 0.055, blue: 0.05),
        textUIColor: UIColor(white: 0.88, alpha: 1),
        tintUIColor: UIColor(white: 0.92, alpha: 1)
    )
}

private enum ReaderFastPaginator {
    static func pages(
        from text: String,
        fontSize: CGFloat,
        pageSize: CGSize
    ) -> [ReaderPage] {
        let paragraphs = text
            .components(separatedBy: "\n\n")
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }

        guard !paragraphs.isEmpty else {
            return [ReaderPage(id: 0, text: "", blocks: [])]
        }

        var pages: [ReaderPage] = []
        var current: [String] = []
        var currentLines = 0
        let budget = PageBudget(fontSize: fontSize, pageSize: pageSize)

        for paragraph in paragraphs {
            if isChapterHeading(paragraph), !current.isEmpty {
                flush(&current, into: &pages)
                currentLines = 0
            }

            let paragraphLines = estimatedLines(for: paragraph, budget: budget)
            let separatorLines = current.isEmpty ? 0 : 1

            if currentLines + separatorLines + paragraphLines <= budget.maxLines {
                current.append(paragraph)
                currentLines += separatorLines + paragraphLines
                continue
            }

            appendSplitting(paragraph, current: &current, currentLines: &currentLines, into: &pages, budget: budget)
        }

        flush(&current, into: &pages)
        return pages.isEmpty ? [ReaderPage(id: 0, text: text, blocks: ReaderTextBlock.blocks(from: text))] : pages
    }

    private static func appendSplitting(
        _ paragraph: String,
        current: inout [String],
        currentLines: inout Int,
        into pages: inout [ReaderPage],
        budget: PageBudget
    ) {
        let words = paragraph.split(separator: " ").map(String.init)
        var index = 0

        while index < words.count {
            let separatorLines = current.isEmpty ? 0 : 1
            let availableLines = budget.maxLines - currentLines - separatorLines

            if availableLines <= 0 {
                flush(&current, into: &pages)
                currentLines = 0
                continue
            }

            var chunk: [String] = []
            var chunkCharacters = 0

            while index < words.count {
                let word = words[index]
                let candidateCharacters = chunkCharacters + (chunk.isEmpty ? 0 : 1) + word.count
                let candidateLines = max(1, Int(ceil(Double(candidateCharacters) / Double(budget.charactersPerLine))))

                if candidateLines > availableLines, !chunk.isEmpty {
                    break
                }

                chunk.append(word)
                chunkCharacters = candidateCharacters
                index += 1

                if candidateLines >= availableLines {
                    break
                }
            }

            if chunk.isEmpty {
                chunk = [words[index]]
                chunkCharacters = words[index].count
                index += 1
            }

            current.append(chunk.joined(separator: " "))
            currentLines += separatorLines + max(1, Int(ceil(Double(chunkCharacters) / Double(budget.charactersPerLine))))

            if index < words.count {
                flush(&current, into: &pages)
                currentLines = 0
            }
        }
    }

    private static func flush(_ paragraphs: inout [String], into pages: inout [ReaderPage]) {
        let text = paragraphs.joined(separator: "\n\n").trimmingCharacters(in: .whitespacesAndNewlines)
        if !text.isEmpty {
            pages.append(ReaderPage(id: pages.count, text: text, blocks: ReaderTextBlock.blocks(from: text)))
        }
        paragraphs = []
    }

    private static func estimatedLines(for paragraph: String, budget: PageBudget) -> Int {
        let characters = max(1, paragraph.count)
        let lines = Int(ceil(Double(characters) / Double(budget.charactersPerLine)))
        if isChapterHeading(paragraph) {
            return lines + 2
        }
        return lines
    }

    private static func isChapterHeading(_ paragraph: String) -> Bool {
        let lowered = paragraph.lowercased()
        return paragraph.hasPrefix("# ")
            || lowered.contains(" h1")
            || lowered.hasPrefix("chapter ")
            || lowered.hasPrefix("chapter:")
            || lowered.hasPrefix("chapitre ")
            || lowered.hasPrefix("chapitre:")
    }

    private struct PageBudget {
        let charactersPerLine: Int
        let maxLines: Int

        init(fontSize: CGFloat, pageSize: CGSize) {
            let averageCharacterWidth = max(6, fontSize * 0.48)
            charactersPerLine = max(18, Int(pageSize.width / averageCharacterWidth))

            let lineHeight = max(20, fontSize * 1.26)
            maxLines = max(8, Int((pageSize.height - 8) / lineHeight))
        }
    }
}

private enum ReaderPaginator {
    static func pages(
        from text: String,
        fontSize: CGFloat,
        fontFamily: ReaderFont,
        pageSize: CGSize
    ) -> [ReaderPage] {
        let paragraphs = text
            .components(separatedBy: "\n\n")
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }

        guard !paragraphs.isEmpty else {
            return [ReaderPage(id: 0, text: "", blocks: [])]
        }

        var pages: [ReaderPage] = []
        var currentParagraphs: [String] = []
        var heightCache: [String: CGFloat] = [:]

        for paragraph in paragraphs {
            if isChapterHeading(paragraph), !currentParagraphs.isEmpty {
                flush(&currentParagraphs, into: &pages)
            }

            if fits(
                currentParagraphs + [paragraph],
                fontSize: fontSize,
                fontFamily: fontFamily,
                pageSize: pageSize,
                heightCache: &heightCache
            ) {
                currentParagraphs.append(paragraph)
                continue
            }

            appendSplitting(
                paragraph,
                currentParagraphs: &currentParagraphs,
                into: &pages,
                fontSize: fontSize,
                fontFamily: fontFamily,
                pageSize: pageSize,
                heightCache: &heightCache
            )
        }

        flush(&currentParagraphs, into: &pages)
        if pages.isEmpty {
            return [ReaderPage(id: 0, text: text, blocks: ReaderTextBlock.blocks(from: text))]
        }
        return pages
    }

    private static func appendSplitting(
        _ paragraph: String,
        currentParagraphs: inout [String],
        into pages: inout [ReaderPage],
        fontSize: CGFloat,
        fontFamily: ReaderFont,
        pageSize: CGSize,
        heightCache: inout [String: CGFloat]
    ) {
        let words = paragraph.split(separator: " ").map(String.init)
        var index = 0

        while index < words.count {
            let fitCount = fittingWordCount(
                words: words,
                startIndex: index,
                baseParagraphs: currentParagraphs,
                fontSize: fontSize,
                fontFamily: fontFamily,
                pageSize: pageSize,
                heightCache: &heightCache
            )

            if fitCount == 0, !currentParagraphs.isEmpty {
                flush(&currentParagraphs, into: &pages)
                continue
            }

            if fitCount > 0 {
                let chunk = words[index..<(index + fitCount)].joined(separator: " ")
                currentParagraphs.append(chunk)
                index += fitCount
                if index < words.count {
                    flush(&currentParagraphs, into: &pages)
                }
            } else {
                currentParagraphs = [words[index]]
                flush(&currentParagraphs, into: &pages)
                index += 1
            }
        }
    }

    private static func flush(_ paragraphs: inout [String], into pages: inout [ReaderPage]) {
        let text = paragraphs.joined(separator: "\n\n").trimmingCharacters(in: .whitespacesAndNewlines)
        if !text.isEmpty {
            pages.append(
                ReaderPage(
                    id: pages.count,
                    text: text,
                    blocks: ReaderTextBlock.blocks(from: text)
                )
            )
        }
        paragraphs = []
    }

    private static func fittingWordCount(
        words: [String],
        startIndex: Int,
        baseParagraphs: [String],
        fontSize: CGFloat,
        fontFamily: ReaderFont,
        pageSize: CGSize,
        heightCache: inout [String: CGFloat]
    ) -> Int {
        var low = 0
        var high = words.count - startIndex
        var best = 0

        while low <= high {
            let middle = (low + high) / 2
            if middle == 0 {
                low = 1
                continue
            }

            let chunk = words[startIndex..<(startIndex + middle)].joined(separator: " ")
            if fits(
                baseParagraphs + [chunk],
                fontSize: fontSize,
                fontFamily: fontFamily,
                pageSize: pageSize,
                heightCache: &heightCache
            ) {
                best = middle
                low = middle + 1
            } else {
                high = middle - 1
            }
        }

        return best
    }

    private static func fits(
        _ paragraphs: [String],
        fontSize: CGFloat,
        fontFamily: ReaderFont,
        pageSize: CGSize,
        heightCache: inout [String: CGFloat]
    ) -> Bool {
        measuredHeight(
            for: paragraphs,
            fontSize: fontSize,
            fontFamily: fontFamily,
            pageSize: pageSize,
            heightCache: &heightCache
        ) <= max(1, pageSize.height - 6)
    }

    private static func measuredHeight(
        for paragraphs: [String],
        fontSize: CGFloat,
        fontFamily: ReaderFont,
        pageSize: CGSize,
        heightCache: inout [String: CGFloat]
    ) -> CGFloat {
        let text = paragraphs.joined(separator: "\n\n").trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else {
            return 0
        }

        if let cached = heightCache[text] {
            return cached
        }

        let attributed = attributedText(
            for: ReaderTextBlock.blocks(from: text),
            fontSize: fontSize,
            fontFamily: fontFamily
        )
        let bounds = attributed.boundingRect(
            with: CGSize(width: max(1, pageSize.width), height: CGFloat.greatestFiniteMagnitude),
            options: [.usesLineFragmentOrigin, .usesFontLeading],
            context: nil
        )
        let height = ceil(bounds.height)
        heightCache[text] = height
        return height
    }

    private static func attributedText(
        for blocks: [ReaderTextBlock],
        fontSize: CGFloat,
        fontFamily: ReaderFont
    ) -> NSAttributedString {
        let output = NSMutableAttributedString()
        for (index, block) in blocks.enumerated() {
            output.append(block.attributedText(fontSize: fontSize, fontFamily: fontFamily, theme: .dark))
            if index < blocks.count - 1 {
                output.append(NSAttributedString(string: "\n"))
            }
        }
        return output
    }

    private static func isChapterHeading(_ paragraph: String) -> Bool {
        let lowered = paragraph.lowercased()
        return paragraph.hasPrefix("# ")
            || lowered.contains(" h1")
            || lowered.hasPrefix("chapter ")
            || lowered.hasPrefix("chapter:")
            || lowered.hasPrefix("chapitre ")
            || lowered.hasPrefix("chapitre:")
    }

}
