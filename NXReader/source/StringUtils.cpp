#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>

namespace nxreader {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool endsWithIgnoreCase(const std::string &value, const std::string &suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }

    return lowerCopy(value).compare(value.size() - suffix.size(), suffix.size(), lowerCopy(suffix)) == 0;
}

std::string directoryName(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return "";
    }
    return path.substr(0, slash);
}

std::string fileName(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string joinPath(const std::string &dir, const std::string &name) {
    if (dir.empty()) {
        return name;
    }
    if (dir[dir.size() - 1] == '/') {
        return dir + name;
    }
    return dir + "/" + name;
}

std::string normalizeZipPath(const std::string &path) {
    std::vector<std::string> parts;
    size_t start = 0;

    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);

        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }

        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    std::string normalized;
    for (size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            normalized += "/";
        }
        normalized += parts[index];
    }
    return normalized;
}

namespace {

constexpr unsigned char kInlineStyleMarker = 0x1f;

const char *styleMarker(char style, bool enabled) {
    static const char italicOn[] = "\x1f"
                                   "I1";
    static const char italicOff[] = "\x1f"
                                    "I0";
    static const char boldOn[] = "\x1f"
                                 "B1";
    static const char boldOff[] = "\x1f"
                                  "B0";

    if (style == 'I') {
        return enabled ? italicOn : italicOff;
    }
    return enabled ? boldOn : boldOff;
}

bool isInlineStyleMarkerAt(const std::string &text, size_t index) {
    if (index + 2 >= text.size()) {
        return false;
    }
    if (static_cast<unsigned char>(text[index]) != kInlineStyleMarker) {
        return false;
    }
    const char style = text[index + 1];
    const char state = text[index + 2];
    return (style == 'I' || style == 'B') && (state == '0' || state == '1');
}

size_t visibleByteEnd(const std::string &text, size_t start, size_t end, size_t maxColumns) {
    size_t index = start;
    size_t visible = 0;
    while (index < end && visible < maxColumns) {
        if (isInlineStyleMarkerAt(text, index)) {
            index += 3;
            continue;
        }

        const unsigned char value = static_cast<unsigned char>(text[index]);
        size_t step = 1;
        if ((value & 0xe0) == 0xc0) {
            step = 2;
        } else if ((value & 0xf0) == 0xe0) {
            step = 3;
        } else if ((value & 0xf8) == 0xf0) {
            step = 4;
        }
        index = std::min(index + step, end);
        visible += 1;
    }
    return index;
}

void updateInlineStyleState(const std::string &text, size_t start, size_t end, bool &bold, bool &italic) {
    for (size_t index = start; index < end; ++index) {
        if (!isInlineStyleMarkerAt(text, index)) {
            continue;
        }

        if (text[index + 1] == 'I') {
            italic = text[index + 2] == '1';
        } else {
            bold = text[index + 2] == '1';
        }
        index += 2;
    }
}

std::string lineWithActiveStyles(const std::string &line, bool bold, bool italic) {
    std::string styled;
    if (bold) {
        styled += styleMarker('B', true);
    }
    if (italic) {
        styled += styleMarker('I', true);
    }
    styled += line;
    return styled;
}

std::string utf8FromCodepoint(unsigned int codepoint) {
    std::string out;
    if (codepoint <= 0x7F) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return out;
}

std::string namedEntityValue(const std::string &name) {
    static const std::map<std::string, std::string> entities = {
        {"amp", "&"},    {"apos", "'"},   {"quot", "\""},    {"lt", "<"},     {"gt", ">"},     {"nbsp", " "},
        {"copy", "(c)"}, {"reg", "(r)"},  {"hellip", "..."}, {"ndash", "-"},  {"mdash", "-"},  {"lsquo", "'"},
        {"rsquo", "'"},  {"ldquo", "\""}, {"rdquo", "\""},   {"laquo", "«"},  {"raquo", "»"},  {"Agrave", "À"},
        {"Aacute", "Á"}, {"Acirc", "Â"},  {"Auml", "Ä"},     {"Ccedil", "Ç"}, {"Egrave", "È"}, {"Eacute", "É"},
        {"Ecirc", "Ê"},  {"Euml", "Ë"},   {"Igrave", "Ì"},   {"Iacute", "Í"}, {"Icirc", "Î"},  {"Iuml", "Ï"},
        {"Ograve", "Ò"}, {"Oacute", "Ó"}, {"Ocirc", "Ô"},    {"Ouml", "Ö"},   {"Ugrave", "Ù"}, {"Uacute", "Ú"},
        {"Ucirc", "Û"},  {"Uuml", "Ü"},   {"agrave", "à"},   {"aacute", "á"}, {"acirc", "â"},  {"auml", "ä"},
        {"ccedil", "ç"}, {"egrave", "è"}, {"eacute", "é"},   {"ecirc", "ê"},  {"euml", "ë"},   {"igrave", "ì"},
        {"iacute", "í"}, {"icirc", "î"},  {"iuml", "ï"},     {"ograve", "ò"}, {"oacute", "ó"}, {"ocirc", "ô"},
        {"ouml", "ö"},   {"ugrave", "ù"}, {"uacute", "ú"},   {"ucirc", "û"},  {"uuml", "ü"},   {"yuml", "ÿ"},
        {"oelig", "œ"},  {"OElig", "Œ"},  {"euro", "€"},
    };

    const auto found = entities.find(name);
    if (found == entities.end()) {
        return "";
    }
    return found->second;
}

std::string removeTagBlock(std::string html, const std::string &tagName) {
    const std::string openNeedle = "<" + tagName;
    const std::string closeNeedle = "</" + tagName + ">";
    std::string lower = lowerCopy(html);
    size_t start = 0;

    while ((start = lower.find(openNeedle, start)) != std::string::npos) {
        const size_t end = lower.find(closeNeedle, start);
        if (end == std::string::npos) {
            html.erase(start);
            break;
        }

        const size_t eraseEnd = end + closeNeedle.size();
        html.erase(start, eraseEnd - start);
        lower.erase(start, eraseEnd - start);
    }

    return html;
}

bool isHeadingTag(const std::string &tag) {
    return tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6";
}

void appendSpace(std::string &text, bool &lastWasSpace) {
    if (!text.empty() && !lastWasSpace && text[text.size() - 1] != '\n') {
        text += ' ';
        lastWasSpace = true;
    }
}

void appendBreak(std::string &text, int count, bool &lastWasSpace) {
    while (!text.empty() && text[text.size() - 1] == ' ') {
        text.pop_back();
    }

    int existing = 0;
    for (size_t index = text.size(); index > 0 && text[index - 1] == '\n'; --index) {
        existing += 1;
    }

    for (int index = existing; index < count; ++index) {
        text += '\n';
    }
    lastWasSpace = true;
}

} // namespace

std::string decodeHtmlEntities(const std::string &text) {
    std::string decoded;
    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '&') {
            decoded += text[index];
            continue;
        }

        const size_t semicolon = text.find(';', index + 1);
        if (semicolon == std::string::npos || semicolon - index > 16) {
            decoded += text[index];
            continue;
        }

        const std::string entity = text.substr(index + 1, semicolon - index - 1);
        std::string replacement;

        if (!entity.empty() && entity[0] == '#') {
            const bool hex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
            const char *numberStart = entity.c_str() + (hex ? 2 : 1);
            char *numberEnd = nullptr;
            const unsigned long codepoint = std::strtoul(numberStart, &numberEnd, hex ? 16 : 10);
            if (numberEnd != numberStart) {
                replacement = utf8FromCodepoint(static_cast<unsigned int>(codepoint));
            }
        } else {
            replacement = namedEntityValue(entity);
        }

        if (replacement.empty()) {
            decoded += '&';
            decoded += entity;
            decoded += ';';
        } else {
            decoded += replacement;
        }
        index = semicolon;
    }

    return decoded;
}

std::string consoleSafeText(const std::string &text) {
    std::string out;
    for (size_t index = 0; index < text.size();) {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (ch < 0x80) {
            out += static_cast<char>(ch);
            index += 1;
            continue;
        }

        std::string replacement = "?";
        const std::string remaining = text.substr(index, std::min<size_t>(4, text.size() - index));
        size_t consumed = 1;

        const auto replace2 = [&](const char *utf8, const char *ascii) {
            if (text.compare(index, std::strlen(utf8), utf8) == 0) {
                replacement = ascii;
                consumed = std::strlen(utf8);
                return true;
            }
            return false;
        };

        if (replace2("à", "a") || replace2("á", "a") || replace2("â", "a") || replace2("ä", "a") ||
            replace2("À", "A") || replace2("Á", "A") || replace2("Â", "A") || replace2("Ä", "A") ||
            replace2("ç", "c") || replace2("Ç", "C") || replace2("è", "e") || replace2("é", "e") ||
            replace2("ê", "e") || replace2("ë", "e") || replace2("È", "E") || replace2("É", "E") ||
            replace2("Ê", "E") || replace2("Ë", "E") || replace2("î", "i") || replace2("ï", "i") ||
            replace2("Î", "I") || replace2("Ï", "I") || replace2("ô", "o") || replace2("ö", "o") ||
            replace2("Ô", "O") || replace2("Ö", "O") || replace2("ù", "u") || replace2("û", "u") ||
            replace2("ü", "u") || replace2("Ù", "U") || replace2("Û", "U") || replace2("Ü", "U") ||
            replace2("œ", "oe") || replace2("Œ", "OE") || replace2("«", "\"") || replace2("»", "\"") ||
            replace2("–", "-") || replace2("—", "-") || replace2("’", "'") || replace2("“", "\"") ||
            replace2("”", "\"")) {
            out += replacement;
            index += consumed;
            continue;
        }

        (void)remaining;
        out += replacement;
        index += consumed;
    }
    return out;
}

std::string stripTagsToText(const std::string &html) {
    std::string cleaned = removeTagBlock(html, "style");
    cleaned = removeTagBlock(cleaned, "script");

    std::string text;
    bool lastWasSpace = true;

    for (size_t index = 0; index < cleaned.size(); ++index) {
        const char ch = cleaned[index];
        if (ch == '<') {
            const size_t tagEnd = cleaned.find('>', index);
            if (tagEnd == std::string::npos) {
                break;
            }

            std::string tag = cleaned.substr(index + 1, tagEnd - index - 1);
            const size_t tagSpace = tag.find_first_of(" \t\r\n");
            if (tagSpace != std::string::npos) {
                tag = tag.substr(0, tagSpace);
            }
            tag = lowerCopy(tag);

            const bool closing = !tag.empty() && tag[0] == '/';
            const std::string bareTag = closing ? tag.substr(1) : tag;

            if (isHeadingTag(bareTag)) {
                appendBreak(text, 2, lastWasSpace);
                if (!closing) {
                    text += "## ";
                    lastWasSpace = false;
                }
            } else if (bareTag == "p" || bareTag == "div" || bareTag == "section" || bareTag == "chapter" ||
                       bareTag == "blockquote") {
                appendBreak(text, 1, lastWasSpace);
            } else if (bareTag == "br" || bareTag == "br/") {
                appendBreak(text, 1, lastWasSpace);
            } else if (bareTag == "li") {
                appendBreak(text, 1, lastWasSpace);
                if (!closing) {
                    text += "- ";
                    lastWasSpace = false;
                }
            } else if (bareTag == "tr") {
                appendBreak(text, 1, lastWasSpace);
            } else if (bareTag == "td" || bareTag == "th") {
                appendSpace(text, lastWasSpace);
            } else if (bareTag == "em" || bareTag == "i") {
                text += styleMarker('I', !closing);
            } else if (bareTag == "strong" || bareTag == "b") {
                text += styleMarker('B', !closing);
            }
            index = tagEnd;
            continue;
        }

        const bool space = std::isspace(static_cast<unsigned char>(ch)) != 0;
        if (space) {
            if (!lastWasSpace) {
                text += ' ';
                lastWasSpace = true;
            }
            continue;
        }

        text += ch;
        lastWasSpace = false;
    }

    return decodeHtmlEntities(text);
}

std::string stripInlineStyleMarkers(const std::string &text) {
    std::string stripped;
    stripped.reserve(text.size());

    for (size_t index = 0; index < text.size(); ++index) {
        if (isInlineStyleMarkerAt(text, index)) {
            index += 2;
            continue;
        }
        stripped += text[index];
    }

    return stripped;
}

std::vector<std::string> paginateText(const std::string &text, size_t charsPerPage) {
    std::vector<std::string> pages;
    size_t start = 0;

    while (start < text.size()) {
        size_t end = std::min(start + charsPerPage, text.size());
        if (end < text.size()) {
            const size_t space = text.rfind(' ', end);
            if (space != std::string::npos && space > start + charsPerPage / 2) {
                end = space;
            }
        }

        pages.push_back(text.substr(start, end - start));
        start = end;
        while (start < text.size() && (text[start] == ' ' || text[start] == '\n')) {
            start += 1;
        }
    }

    if (pages.empty()) {
        pages.push_back("This chapter has no readable text yet.");
    }

    return pages;
}

std::vector<std::string> wrapTextLines(const std::string &text, size_t maxColumns) {
    std::vector<std::string> lines;
    size_t lineStart = 0;

    while (lineStart < text.size()) {
        if (text[lineStart] == '\n') {
            lines.push_back("");
            lineStart += 1;
            continue;
        }

        size_t hardBreak = text.find('\n', lineStart);
        size_t segmentEnd = hardBreak == std::string::npos ? text.size() : hardBreak;
        bool activeBold = false;
        bool activeItalic = false;

        while (lineStart < segmentEnd) {
            size_t lineEnd = visibleByteEnd(text, lineStart, segmentEnd, maxColumns);
            if (lineEnd <= lineStart) {
                lineEnd = std::min(lineStart + maxColumns, segmentEnd);
            }
            if (lineEnd < segmentEnd) {
                const size_t space = text.rfind(' ', lineEnd);
                if (space != std::string::npos && space > lineStart) {
                    lineEnd = space;
                }
            }

            lines.push_back(lineWithActiveStyles(text.substr(lineStart, lineEnd - lineStart), activeBold, activeItalic));
            updateInlineStyleState(text, lineStart, lineEnd, activeBold, activeItalic);
            lineStart = lineEnd;
            while (lineStart < segmentEnd && text[lineStart] == ' ') {
                lineStart += 1;
            }
        }

        if (hardBreak == std::string::npos) {
            break;
        }
        lineStart = hardBreak + 1;
    }

    if (lines.empty()) {
        lines.push_back("");
    }

    return lines;
}

} // namespace nxreader
