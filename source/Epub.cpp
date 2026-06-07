#include "nxreader/Epub.hpp"

#include "nxreader/StringUtils.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <zlib.h>

namespace nxreader {
namespace {

struct ZipEntry {
    std::string name;
    unsigned short method = 0;
    unsigned int compressedSize = 0;
    unsigned int uncompressedSize = 0;
    unsigned int localHeaderOffset = 0;
};

unsigned short readU16(const unsigned char* data) {
    return static_cast<unsigned short>(data[0] | (data[1] << 8));
}

unsigned int readU32(const unsigned char* data) {
    return static_cast<unsigned int>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

bool readFileRange(FILE* file, long offset, size_t size, std::vector<unsigned char>& out) {
    out.assign(size, 0);
    if (std::fseek(file, offset, SEEK_SET) != 0) {
        return false;
    }
    return std::fread(out.data(), 1, size, file) == size;
}

class ZipArchive {
public:
    bool open(const std::string& path, std::string& error) {
        struct stat info;
        if (stat(path.c_str(), &info) != 0) {
            error = "Could not stat EPUB path:\n" + path + "\n\n" + std::strerror(errno);
            return false;
        }

        file_ = std::fopen(path.c_str(), "rb");
        if (file_ == nullptr) {
            error = "Could not open EPUB file:\n" + path + "\n\nSize from stat: " +
                    std::to_string(static_cast<long long>(info.st_size)) + " bytes\n" + std::strerror(errno);
            return false;
        }

        if (!readCentralDirectory(error)) {
            close();
            return false;
        }

        return true;
    }

    void close() {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    bool readText(const std::string& name, std::string& text, std::string& error) {
        std::vector<unsigned char> bytes;
        if (!readBytes(name, bytes, error)) {
            return false;
        }

        text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true;
    }

    bool readBytes(const std::string& name, std::vector<unsigned char>& out, std::string& error) {
        const ZipEntry* entry = findEntry(name);
        if (entry == nullptr) {
            error = "Missing EPUB entry: " + name;
            return false;
        }

        if (entry->method != 0 && entry->method != 8) {
            error = "Unsupported ZIP compression method in: " + name;
            return false;
        }

        std::vector<unsigned char> header;
        if (!readFileRange(file_, entry->localHeaderOffset, 30, header) || readU32(header.data()) != 0x04034b50) {
            error = "Invalid local ZIP header for: " + name;
            return false;
        }

        const unsigned short nameLength = readU16(header.data() + 26);
        const unsigned short extraLength = readU16(header.data() + 28);
        const long dataOffset = static_cast<long>(entry->localHeaderOffset) + 30 + nameLength + extraLength;

        std::vector<unsigned char> bytes;
        if (!readFileRange(file_, dataOffset, entry->compressedSize, bytes)) {
            error = "Could not read ZIP entry: " + name;
            return false;
        }

        if (entry->method == 0) {
            out = bytes;
            return true;
        }

        std::vector<unsigned char> inflated(entry->uncompressedSize);
        z_stream stream = {};
        stream.next_in = bytes.data();
        stream.avail_in = static_cast<uInt>(bytes.size());
        stream.next_out = inflated.data();
        stream.avail_out = static_cast<uInt>(inflated.size());

        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            error = "Could not initialize deflate reader.";
            return false;
        }

        const int result = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);

        if (result != Z_STREAM_END) {
            error = "Could not decompress ZIP entry: " + name;
            return false;
        }

        out.assign(inflated.begin(), inflated.begin() + stream.total_out);
        return true;
    }

private:
    FILE* file_ = nullptr;
    std::vector<ZipEntry> entries_;

    const ZipEntry* findEntry(const std::string& name) const {
        const std::string normalized = normalizeZipPath(name);
        for (const ZipEntry& entry : entries_) {
            if (entry.name == normalized) {
                return &entry;
            }
        }
        return nullptr;
    }

    bool readCentralDirectory(std::string& error) {
        if (std::fseek(file_, 0, SEEK_END) != 0) {
            error = "Could not seek EPUB file.";
            return false;
        }

        const long fileSize = std::ftell(file_);
        const long searchSize = std::min<long>(fileSize, 66000);
        std::vector<unsigned char> tail;
        if (!readFileRange(file_, fileSize - searchSize, searchSize, tail)) {
            error = "Could not read ZIP directory.";
            return false;
        }

        long eocdOffset = -1;
        for (long index = searchSize - 22; index >= 0; --index) {
            if (readU32(tail.data() + index) == 0x06054b50) {
                eocdOffset = fileSize - searchSize + index;
                break;
            }
        }

        if (eocdOffset < 0) {
            error = "This does not look like an EPUB ZIP file.";
            return false;
        }

        const unsigned char* eocd = tail.data() + (eocdOffset - (fileSize - searchSize));
        const unsigned short entryCount = readU16(eocd + 10);
        const unsigned int centralOffset = readU32(eocd + 16);

        if (std::fseek(file_, centralOffset, SEEK_SET) != 0) {
            error = "Could not seek ZIP central directory.";
            return false;
        }

        entries_.clear();
        for (unsigned short index = 0; index < entryCount; ++index) {
            unsigned char header[46] = {};
            if (std::fread(header, 1, sizeof(header), file_) != sizeof(header) || readU32(header) != 0x02014b50) {
                error = "Invalid ZIP central directory.";
                return false;
            }

            const unsigned short nameLength = readU16(header + 28);
            const unsigned short extraLength = readU16(header + 30);
            const unsigned short commentLength = readU16(header + 32);

            std::vector<unsigned char> nameBytes(nameLength);
            if (std::fread(nameBytes.data(), 1, nameLength, file_) != nameLength) {
                error = "Could not read ZIP entry name.";
                return false;
            }

            ZipEntry entry;
            entry.name.assign(reinterpret_cast<const char*>(nameBytes.data()), nameBytes.size());
            entry.method = readU16(header + 10);
            entry.compressedSize = readU32(header + 20);
            entry.uncompressedSize = readU32(header + 24);
            entry.localHeaderOffset = readU32(header + 42);
            entries_.push_back(entry);

            if (std::fseek(file_, extraLength + commentLength, SEEK_CUR) != 0) {
                error = "Could not skip ZIP metadata.";
                return false;
            }
        }

        return true;
    }
};

std::string attrValue(const std::string& xml, size_t tagStart, const std::string& attrName) {
    const size_t tagEnd = xml.find('>', tagStart);
    if (tagEnd == std::string::npos) {
        return "";
    }

    const size_t nameStart = xml.find(attrName, tagStart);
    if (nameStart == std::string::npos || nameStart > tagEnd) {
        return "";
    }

    size_t cursor = nameStart + attrName.size();
    while (cursor < tagEnd && std::isspace(static_cast<unsigned char>(xml[cursor]))) {
        cursor += 1;
    }

    if (cursor >= tagEnd || xml[cursor] != '=') {
        return "";
    }

    cursor += 1;
    while (cursor < tagEnd && std::isspace(static_cast<unsigned char>(xml[cursor]))) {
        cursor += 1;
    }

    if (xml[cursor] == '"' || xml[cursor] == '\'') {
        const char quote = xml[cursor];
        const size_t contentStart = cursor + 1;
        const size_t contentEnd = xml.find(quote, contentStart);
        if (contentEnd == std::string::npos || contentEnd > tagEnd) {
            return "";
        }

        return xml.substr(contentStart, contentEnd - contentStart);
    }

    const size_t contentStart = cursor;
    while (cursor < tagEnd && !std::isspace(static_cast<unsigned char>(xml[cursor])) && xml[cursor] != '/' && xml[cursor] != '>') {
        cursor += 1;
    }

    return xml.substr(contentStart, cursor - contentStart);
}

std::string firstTagText(const std::string& xml, const std::string& tagName) {
    const size_t startTag = xml.find("<" + tagName);
    if (startTag == std::string::npos) {
        return "";
    }

    const size_t startClose = xml.find('>', startTag);
    if (startClose == std::string::npos) {
        return "";
    }

    const size_t endTag = xml.find("</" + tagName + ">", startClose);
    if (endTag == std::string::npos) {
        return "";
    }

    return stripTagsToText(xml.substr(startClose + 1, endTag - startClose - 1));
}

std::string findRootfilePath(const std::string& containerXml) {
    size_t cursor = 0;
    while ((cursor = containerXml.find("<rootfile", cursor)) != std::string::npos) {
        const size_t afterName = cursor + std::strlen("<rootfile");
        if (afterName < containerXml.size() &&
            (std::isspace(static_cast<unsigned char>(containerXml[afterName])) ||
             containerXml[afterName] == '/' ||
             containerXml[afterName] == '>')) {
            return attrValue(containerXml, cursor, "full-path");
        }
        cursor = afterName;
    }
    return "";
}

bool isImageMediaType(const std::string& mediaType) {
    return mediaType.rfind("image/", 0) == 0;
}

std::string imageMarker(const std::string& imagePath) {
    return "<br/>[[NXREADER_IMAGE:" + imagePath + "]]<br/>";
}

std::string htmlWithImageMarkers(const std::string& html, const std::string& chapterPath) {
    std::string out;
    const std::string chapterDir = directoryName(chapterPath);
    size_t cursor = 0;

    while (cursor < html.size()) {
        const size_t imgStart = html.find("<img", cursor);
        if (imgStart == std::string::npos) {
            out += html.substr(cursor);
            break;
        }

        out += html.substr(cursor, imgStart - cursor);
        const size_t imgEnd = html.find('>', imgStart);
        if (imgEnd == std::string::npos) {
            break;
        }

        const std::string src = attrValue(html, imgStart, "src");
        if (!src.empty() && src.rfind("data:", 0) != 0) {
            out += imageMarker(normalizeZipPath(joinPath(chapterDir, src)));
        }

        cursor = imgEnd + 1;
    }

    return out;
}

void addImageIfReadable(ZipArchive& zip, EpubBook& book, std::set<std::string>& seenImages, const std::string& href,
                        const std::string& mediaType) {
    if (href.empty() || seenImages.find(href) != seenImages.end()) {
        return;
    }

    std::string ignoredError;
    EpubImage image;
    image.href = href;
    image.mediaType = mediaType;
    if (zip.readBytes(href, image.bytes, ignoredError) && !image.bytes.empty()) {
        book.images.push_back(image);
        seenImages.insert(href);
    }
}

std::vector<std::string> extractImageMarkers(const std::string& text) {
    std::vector<std::string> paths;
    size_t cursor = 0;
    constexpr const char* marker = "[[NXREADER_IMAGE:";
    constexpr size_t markerLength = 17;

    while ((cursor = text.find(marker, cursor)) != std::string::npos) {
        const size_t pathStart = cursor + markerLength;
        const size_t pathEnd = text.find("]]", pathStart);
        if (pathEnd == std::string::npos) {
            break;
        }
        paths.push_back(text.substr(pathStart, pathEnd - pathStart));
        cursor = pathEnd + 2;
    }

    return paths;
}

}  // namespace

bool loadEpub(const std::string& path, EpubBook& book, std::string& error) {
    book = {};
    book.path = path;

    ZipArchive zip;
    if (!zip.open(path, error)) {
        return false;
    }

    std::string containerXml;
    if (!zip.readText("META-INF/container.xml", containerXml, error)) {
        zip.close();
        return false;
    }

    const std::string opfPath = findRootfilePath(containerXml);
    if (opfPath.empty()) {
        zip.close();
        error = "EPUB container.xml did not contain a rootfile path.\n\nPreview:\n" +
                containerXml.substr(0, 500);
        return false;
    }

    std::string opfXml;
    if (!zip.readText(opfPath, opfXml, error)) {
        zip.close();
        return false;
    }

    book.title = firstTagText(opfXml, "dc:title");
    if (book.title.empty()) {
        book.title = fileName(path);
    }

    struct ManifestItem {
        std::string id;
        std::string href;
        std::string mediaType;
        std::string properties;
    };

    std::map<std::string, ManifestItem> manifest;
    std::string coverId;
    size_t metaStart = 0;
    while ((metaStart = opfXml.find("<meta", metaStart)) != std::string::npos) {
        if (attrValue(opfXml, metaStart, "name") == "cover") {
            coverId = attrValue(opfXml, metaStart, "content");
        }
        metaStart += 5;
    }

    size_t itemStart = 0;
    while ((itemStart = opfXml.find("<item", itemStart)) != std::string::npos) {
        const std::string id = attrValue(opfXml, itemStart, "id");
        const std::string href = attrValue(opfXml, itemStart, "href");
        const std::string mediaType = attrValue(opfXml, itemStart, "media-type");
        const std::string properties = attrValue(opfXml, itemStart, "properties");
        if (!id.empty() && !href.empty()) {
            manifest[id] = {id, href, mediaType, properties};
        }
        itemStart += 5;
    }

    const std::string opfDir = directoryName(opfPath);
    std::set<std::string> seenImages;
    const ManifestItem* coverItem = nullptr;
    for (const auto& pair : manifest) {
        const ManifestItem& item = pair.second;
        if (!isImageMediaType(item.mediaType)) {
            continue;
        }

        const std::string lowerId = lowerCopy(item.id);
        const std::string lowerHref = lowerCopy(item.href);
        if ((!coverId.empty() && item.id == coverId) ||
            item.properties.find("cover-image") != std::string::npos ||
            lowerId.find("cover") != std::string::npos ||
            lowerHref.find("cover") != std::string::npos) {
            coverItem = &item;
            break;
        }
    }

    if (coverItem != nullptr) {
        book.coverImage.href = normalizeZipPath(joinPath(opfDir, coverItem->href));
        book.coverImage.mediaType = coverItem->mediaType;
        zip.readBytes(book.coverImage.href, book.coverImage.bytes, error);
        error.clear();
        if (!book.coverImage.bytes.empty()) {
            book.images.push_back(book.coverImage);
            seenImages.insert(book.coverImage.href);
        }
    }

    size_t itemRefStart = 0;
    while ((itemRefStart = opfXml.find("<itemref", itemRefStart)) != std::string::npos) {
        const std::string idref = attrValue(opfXml, itemRefStart, "idref");
        const auto found = manifest.find(idref);
        if (found != manifest.end()) {
            const ManifestItem& item = found->second;
            if (item.mediaType.find("html") != std::string::npos || endsWithIgnoreCase(item.href, ".xhtml")) {
                const std::string chapterPath = normalizeZipPath(joinPath(opfDir, item.href));
                std::string html;
                EpubChapter chapter;
                chapter.href = chapterPath;
                chapter.mediaType = item.mediaType;
                if (zip.readText(chapterPath, html, error)) {
                    chapter.text = stripTagsToText(htmlWithImageMarkers(html, chapterPath));
                    for (const std::string& imagePath : extractImageMarkers(chapter.text)) {
                        std::string mediaType;
                        for (const auto& pair : manifest) {
                            const ManifestItem& manifestItem = pair.second;
                            const std::string manifestPath = normalizeZipPath(joinPath(opfDir, manifestItem.href));
                            if (manifestPath == imagePath) {
                                mediaType = manifestItem.mediaType;
                                break;
                            }
                        }
                        addImageIfReadable(zip, book, seenImages, imagePath, mediaType);
                    }
                } else {
                    chapter.text = error;
                    zip.close();
                    return false;
                }
                book.chapters.push_back(chapter);
            }
        }
        itemRefStart += 8;
    }

    zip.close();

    if (book.chapters.empty()) {
        error = "No readable XHTML chapters found in this EPUB.";
        return false;
    }

    return true;
}

}  // namespace nxreader
