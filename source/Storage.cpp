#include "nxreader/Storage.hpp"

#include "nxreader/Constants.hpp"
#include "nxreader/Reader.hpp"
#include "nxreader/Settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace nxreader {

void ensureSaveDirExists() {
    mkdir("sdmc:/switch", 0777);
    mkdir(kSaveDir, 0777);
    mkdir(kFontsRoot, 0777);
}

namespace {

unsigned int fnv1a(const char* value) {
    unsigned int hash = 2166136261u;
    for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value); *cursor != 0; ++cursor) {
        hash ^= *cursor;
        hash *= 16777619u;
    }
    return hash;
}

std::string progressPath(const char* bookPath) {
    char path[128] = {};
    std::snprintf(path, sizeof(path), "%s/progress_%08x.txt", kSaveDir, fnv1a(bookPath));
    return path;
}

std::string annotationsPath(const char* bookPath) {
    char path[128] = {};
    std::snprintf(path, sizeof(path), "%s/annotations_%08x.tsv", kSaveDir, fnv1a(bookPath));
    return path;
}

std::string escapeField(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        if (ch == '\\') {
            escaped += "\\\\";
        } else if (ch == '\t') {
            escaped += "\\t";
        } else if (ch == '\n' || ch == '\r') {
            escaped += "\\n";
        } else {
            escaped += ch;
        }
    }
    return escaped;
}

std::string unescapeField(const std::string& value) {
    std::string unescaped;
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\\' && index + 1 < value.size()) {
            const char next = value[index + 1];
            if (next == 't') {
                unescaped += '\t';
                index += 1;
            } else if (next == 'n') {
                unescaped += '\n';
                index += 1;
            } else if (next == '\\') {
                unescaped += '\\';
                index += 1;
            } else {
                unescaped += value[index];
            }
        } else {
            unescaped += value[index];
        }
    }
    return unescaped;
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t end = line.find('\t', start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

}  // namespace

int loadLastPage(const char* bookPath, int maxPage) {
    const std::string path = progressPath(bookPath == nullptr ? "" : bookPath);
    FILE* file = std::fopen(path.c_str(), "r");
    if (file == nullptr) {
        return 1;
    }

    int page = 1;
    if (std::fscanf(file, "%d", &page) != 1) {
        page = 1;
    }

    std::fclose(file);

    if (page < 1) {
        return 1;
    }

    if (page > maxPage) {
        return maxPage;
    }

    return page;
}

void saveLastPage(const char* bookPath, int page) {
    ensureSaveDirExists();

    const std::string path = progressPath(bookPath == nullptr ? "" : bookPath);
    FILE* file = std::fopen(path.c_str(), "w");
    if (file == nullptr) {
        return;
    }

    std::fprintf(file, "%d\n%s\n", page, bookPath == nullptr ? "" : bookPath);
    std::fclose(file);
}

AppSettings loadSettings() {
    AppSettings settings;
    FILE* file = std::fopen("sdmc:/switch/NXReader/settings.txt", "r");
    if (file == nullptr) {
        return settings;
    }

    int fontSize = settings.fontSize;
    int fontIndex = settings.fontIndex;
    int darkMode = settings.darkMode ? 1 : 0;
    int showHeaderOnTurn = settings.showHeaderOnTurn ? 1 : 0;
    int animatePageTurns = settings.animatePageTurns ? 1 : 0;
    std::fscanf(file,
                "fontSize=%d\nfontIndex=%d\ndarkMode=%d\nshowHeaderOnTurn=%d\nanimatePageTurns=%d",
                &fontSize,
                &fontIndex,
                &darkMode,
                &showHeaderOnTurn,
                &animatePageTurns);
    std::fclose(file);

    settings.fontSize = clampFontSize(fontSize);
    settings.fontIndex = clampFontIndex(fontIndex);
    settings.darkMode = darkMode != 0;
    settings.showHeaderOnTurn = showHeaderOnTurn != 0;
    settings.animatePageTurns = animatePageTurns != 0;
    return settings;
}

void saveSettings(const AppSettings& settings) {
    ensureSaveDirExists();

    FILE* file = std::fopen("sdmc:/switch/NXReader/settings.txt", "w");
    if (file == nullptr) {
        return;
    }

    std::fprintf(file, "fontSize=%d\nfontIndex=%d\ndarkMode=%d\nshowHeaderOnTurn=%d\nanimatePageTurns=%d\n",
                 clampFontSize(settings.fontSize),
                 clampFontIndex(settings.fontIndex),
                 settings.darkMode ? 1 : 0,
                 settings.showHeaderOnTurn ? 1 : 0,
                 settings.animatePageTurns ? 1 : 0);
    std::fclose(file);
}

std::vector<Annotation> loadAnnotations(const char* bookPath) {
    std::vector<Annotation> annotations;
    FILE* file = std::fopen(annotationsPath(bookPath == nullptr ? "" : bookPath).c_str(), "r");
    if (file == nullptr) {
        return annotations;
    }

    char line[4096] = {};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        std::string textLine = line;
        while (!textLine.empty() && (textLine.back() == '\n' || textLine.back() == '\r')) {
            textLine.pop_back();
        }
        const std::vector<std::string> fields = splitTabs(textLine);
        if (fields.size() < 4) {
            continue;
        }

        Annotation annotation;
        annotation.page = std::max(1, std::atoi(fields[0].c_str()));
        annotation.text = unescapeField(fields[1]);
        annotation.translation = unescapeField(fields[2]);
        annotation.note = unescapeField(fields[3]);
        if (!annotation.text.empty()) {
            annotations.push_back(annotation);
        }
    }

    std::fclose(file);
    return annotations;
}

void saveAnnotations(const char* bookPath, const std::vector<Annotation>& annotations) {
    ensureSaveDirExists();

    FILE* file = std::fopen(annotationsPath(bookPath == nullptr ? "" : bookPath).c_str(), "w");
    if (file == nullptr) {
        return;
    }

    for (const Annotation& annotation : annotations) {
        std::fprintf(file,
                     "%d\t%s\t%s\t%s\n",
                     std::max(1, annotation.page),
                     escapeField(annotation.text).c_str(),
                     escapeField(annotation.translation).c_str(),
                     escapeField(annotation.note).c_str());
    }

    std::fclose(file);
}

}  // namespace nxreader
