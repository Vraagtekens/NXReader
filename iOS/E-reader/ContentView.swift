import SwiftUI

enum ReaderTab: String, CaseIterable {
    case home = "Home"
    case library = "Library"
    case notes = "Notes"

    var icon: String {
        switch self {
        case .home: "house.fill"
        case .library: "books.vertical.fill"
        case .notes: "quote.bubble.fill"
        }
    }
}

struct ContentView: View {
    @StateObject private var store = LibraryStore()

    var body: some View {
        TabView {
            NavigationStack {
                HomeView(
                    store: store,
                    books: store.books.sorted { $0.lastOpenedAt > $1.lastOpenedAt },
                    readingDays: store.readingDays
                )
            }
            .tabItem {
                Label(ReaderTab.home.rawValue, systemImage: ReaderTab.home.icon)
            }

            NavigationStack {
                LibraryView(store: store)
            }
            .tabItem {
                Label(ReaderTab.library.rawValue, systemImage: ReaderTab.library.icon)
            }

            NavigationStack {
                NotesView(notes: store.notes.sorted { $0.createdAt > $1.createdAt })
            }
            .tabItem {
                Label(ReaderTab.notes.rawValue, systemImage: ReaderTab.notes.icon)
            }
        }
        .tint(.primary)
    }
}

struct GlassTabBar: View {
    @Binding var selectedTab: ReaderTab

    var body: some View {
        HStack(spacing: 6) {
            ForEach(ReaderTab.allCases, id: \.self) { tab in
                Button {
                    withAnimation(.spring(response: 0.28, dampingFraction: 0.82)) {
                        selectedTab = tab
                    }
                } label: {
                    VStack(spacing: 4) {
                        Image(systemName: tab.icon)
                            .font(.system(size: 18, weight: .semibold))
                        Text(tab.rawValue)
                            .font(.caption2.weight(.semibold))
                    }
                    .frame(maxWidth: .infinity)
                    .frame(height: 54)
                    .foregroundStyle(selectedTab == tab ? Color.primary : Color.secondary)
                    .background {
                        if selectedTab == tab {
                            Capsule()
                                .fill(.white.opacity(0.62))
                                .shadow(color: .black.opacity(0.10), radius: 10, y: 5)
                        }
                    }
                }
                .buttonStyle(.plain)
            }
        }
        .padding(8)
        .frame(maxWidth: 430)
        .background(.ultraThinMaterial, in: Capsule())
        .overlay {
            Capsule()
                .stroke(.white.opacity(0.45), lineWidth: 1)
        }
        .shadow(color: .black.opacity(0.16), radius: 24, y: 12)
        .padding(.horizontal, 18)
        .padding(.bottom, 10)
    }
}
