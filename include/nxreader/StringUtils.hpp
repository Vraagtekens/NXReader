#pragma once

#include <string>
#include <vector>

namespace nxreader {

bool endsWithIgnoreCase(const std::string &value, const std::string &suffix);
std::string directoryName(const std::string &path);
std::string fileName(const std::string &path);
std::string joinPath(const std::string &dir, const std::string &name);
std::string lowerCopy(std::string value);
std::string normalizeZipPath(const std::string &path);
std::string consoleSafeText(const std::string &text);
std::string decodeHtmlEntities(const std::string &text);
std::string stripTagsToText(const std::string &html);
std::vector<std::string> paginateText(const std::string &text, size_t charsPerPage);
std::vector<std::string> wrapTextLines(const std::string &text, size_t maxColumns);

} // namespace nxreader
