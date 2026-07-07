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

    private var fontSize: CGFloat {
        CGFloat(storedFontSize)
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
                        highlights: book.highlights,
                        selectedText: $selectedText
                    )
                    .tag(index)
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
                    pageIndex: pageIndex,
                    pageCount: max(1, pages.count),
                    selectedText: selectedText,
                    isMenuExpanded: $isMenuExpanded,
                    lockRotation: lockRotation,
                    onBack: { dismiss() },
                    onHighlight: highlightSelection,
                    onContents: { activeSheet = .contents },
                    onSettings: { activeSheet = .settings },
                    onToggleRotationLock: { lockRotation.toggle() }
                )
                .transition(.opacity)
            }
        }
        .preferredColorScheme(useDarkMode ? .dark : .light)
        .navigationBarBackButtonHidden(true)
        .toolbar(.hidden, for: .navigationBar)
        .toolbar(.hidden, for: .tabBar)
        .onAppear {
            rebuildPages()
            book.lastOpenedAt = .now
            pageIndex = min(max(book.currentPage - 1, 0), max(0, pages.count - 1))
            book.pageCount = max(1, pages.count)
            OrientationController.shared.setRotationLocked(lockRotation)
        }
        .onChange(of: pageIndex) { _, newValue in
            selectedText = ""
            book.currentPage = newValue + 1
            book.pageCount = max(1, pages.count)
        }
        .onChange(of: storedFontSize) { _, _ in
            rebuildPages()
            pageIndex = min(pageIndex, max(0, pages.count - 1))
            book.pageCount = max(1, pages.count)
        }
        .onChange(of: storedFontFamily) { _, _ in
            rebuildPages()
            pageIndex = min(pageIndex, max(0, pages.count - 1))
            book.pageCount = max(1, pages.count)
        }
        .onChange(of: book.sampleText) { _, _ in
            rebuildPages()
            pageIndex = min(pageIndex, max(0, pages.count - 1))
            book.pageCount = max(1, pages.count)
        }
        .onChange(of: lockRotation) { _, newValue in
            OrientationController.shared.setRotationLocked(newValue)
        }
        .onDisappear {
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
                    storedFontSize: $storedFontSize,
                    storedFontFamily: $storedFontFamily,
                    useDarkMode: $useDarkMode
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

    private func rebuildPages() {
        renderedPages = ReaderPaginator.pages(
            from: book.sampleText,
            targetCharacters: ReaderPaginator.targetCharacters(for: fontSize)
        )
    }
}

private enum ReaderSheet: String, Identifiable {
    case contents
    case settings

    var id: String { rawValue }
}

private struct ReaderChrome: View {
    let title: String
    let pageIndex: Int
    let pageCount: Int
    let selectedText: String
    @Binding var isMenuExpanded: Bool
    let lockRotation: Bool
    let onBack: () -> Void
    let onHighlight: () -> Void
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
                Button(action: onHighlight) {
                    Label("Highlight", systemImage: "highlighter")
                        .font(.caption.weight(.semibold))
                        .padding(.horizontal, 14)
                        .padding(.vertical, 9)
                        .background(.regularMaterial, in: Capsule())
                }
                .buttonStyle(.plain)
                .padding(.top, 8)
            }

            Spacer()

            HStack(alignment: .bottom) {
                Spacer()

                Text("Page \(pageIndex + 1) of \(pageCount)")
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

private struct ReaderPageView: View {
    let page: ReaderPage
    let fontSize: CGFloat
    let fontFamily: ReaderFont
    let theme: ReaderTheme
    let highlights: [String]
    @Binding var selectedText: String

    var body: some View {
        SelectableReaderText(
            blocks: page.blocks,
            fontSize: fontSize,
            fontFamily: fontFamily,
            theme: theme,
            highlights: highlights,
            selectedText: $selectedText
        )
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .padding(.horizontal, 20)
        .padding(.top, 86)
        .padding(.bottom, 82)
    }
}

private struct SelectableReaderText: UIViewRepresentable {
    let blocks: [ReaderTextBlock]
    let fontSize: CGFloat
    let fontFamily: ReaderFont
    let theme: ReaderTheme
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
        textView.textContainer.widthTracksTextView = true
        textView.textContainer.lineBreakMode = .byWordWrapping
        textView.showsVerticalScrollIndicator = false
        textView.adjustsFontForContentSizeCategory = false
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
            height: CGFloat.greatestFiniteMagnitude
        )
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(parent: self)
    }

    private func attributedText() -> NSAttributedString {
        let output = NSMutableAttributedString()

        for (index, block) in blocks.enumerated() {
            output.append(block.attributedText(fontSize: fontSize, fontFamily: fontFamily, theme: theme))
            if index < blocks.count - 1 {
                output.append(NSAttributedString(string: "\n\n"))
            }
        }

        applyHighlights(to: output)
        return output
    }

    private var renderSignature: String {
        [
            blocks.map { "\($0.kind.signature):\($0.markdown)" }.joined(separator: "|"),
            String(Double(fontSize)),
            fontFamily.rawValue,
            theme.id,
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
        }
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
                            Text("Page \(chapter.pageIndex + 1)")
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
    @Binding var storedFontSize: Double
    @Binding var storedFontFamily: String
    @Binding var useDarkMode: Bool

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack {
                        Button {
                            storedFontSize = max(16, storedFontSize - 2)
                        } label: {
                            Image(systemName: "textformat.size.smaller")
                                .frame(width: 34, height: 34)
                        }
                        .buttonStyle(.borderless)

                        Slider(value: $storedFontSize, in: 16...34, step: 1)

                        Button {
                            storedFontSize = min(34, storedFontSize + 2)
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
        case listItem
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
        paragraph.alignment = .natural
        paragraph.lineBreakMode = .byWordWrapping

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
        case .listItem, .paragraph:
            9
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
                case .listItem:
                    ReaderTextBlock(kind: .listItem, markdown: listBody(raw))
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
        if raw.hasPrefix("- ") || lowered.hasPrefix("liste:") {
            return .listItem
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
        case .listItem: "li"
        case .paragraph: "p"
        }
    }

    var isHeading: Bool {
        switch self {
        case .heading1, .heading2, .heading3:
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
            return ReaderChapter(title: heading.plainTitle, pageIndex: index)
        }
        return chapters.isEmpty ? [ReaderChapter(title: "Start", pageIndex: 0)] : chapters
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

private enum ReaderPaginator {
    static func targetCharacters(for fontSize: CGFloat) -> Int {
        let base = 1_350
        let adjusted = Double(base) * pow(22 / Double(fontSize), 1.65)
        return max(520, min(1_800, Int(adjusted)))
    }

    static func pages(from text: String, targetCharacters: Int) -> [ReaderPage] {
        let paragraphs = text
            .components(separatedBy: "\n\n")
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
            .filter { !$0.isEmpty }

        guard !paragraphs.isEmpty else {
            return [ReaderPage(id: 0, text: "", blocks: [])]
        }

        var pages: [ReaderPage] = []
        var current = ""

        for paragraph in paragraphs {
            let paragraphTarget = paragraph.hasPrefix("#") ? max(120, targetCharacters / 3) : targetCharacters
            if paragraph.count > paragraphTarget {
                flush(&current, into: &pages)
                split(paragraph, targetCharacters: targetCharacters, into: &pages)
                continue
            }

            let separator = current.isEmpty ? "" : "\n\n"
            if current.count + separator.count + paragraph.count > targetCharacters {
                flush(&current, into: &pages)
            }

            current += (current.isEmpty ? "" : "\n\n") + paragraph
        }

        flush(&current, into: &pages)
        if pages.isEmpty {
            return [ReaderPage(id: 0, text: text, blocks: ReaderTextBlock.blocks(from: text))]
        }
        return pages
    }

    private static func flush(_ current: inout String, into pages: inout [ReaderPage]) {
        let text = current.trimmingCharacters(in: .whitespacesAndNewlines)
        if !text.isEmpty {
            pages.append(
                ReaderPage(
                    id: pages.count,
                    text: text,
                    blocks: ReaderTextBlock.blocks(from: text)
                )
            )
        }
        current = ""
    }

    private static func split(
        _ paragraph: String,
        targetCharacters: Int,
        into pages: inout [ReaderPage]
    ) {
        var chunk = ""
        for word in paragraph.split(separator: " ") {
            let next = chunk.isEmpty ? String(word) : "\(chunk) \(word)"
            if next.count > targetCharacters {
                flush(&chunk, into: &pages)
                chunk = String(word)
            } else {
                chunk = next
            }
        }
        flush(&chunk, into: &pages)
    }
}
