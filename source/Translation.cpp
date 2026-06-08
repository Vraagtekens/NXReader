#include "nxreader/Translation.hpp"

#include <algorithm>
#include <cstdio>
#include <curl/curl.h>
#include <string>

namespace nxreader {
namespace {

size_t writeResponse(char *data, size_t size, size_t nmemb, void *userdata) {
    std::string *response = static_cast<std::string *>(userdata);
    response->append(data, size * nmemb);
    return size * nmemb;
}

void appendUtf8(std::string &output, unsigned int codepoint) {
    if (codepoint <= 0x7f) {
        output += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7ff) {
        output += static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f));
        output += static_cast<char>(0x80 | (codepoint & 0x3f));
    } else {
        output += static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        output += static_cast<char>(0x80 | (codepoint & 0x3f));
    }
}

int hexValue(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return 10 + value - 'a';
    }
    if (value >= 'A' && value <= 'F') {
        return 10 + value - 'A';
    }
    return -1;
}

std::string unescapeJsonString(const std::string &value) {
    std::string output;
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\' || index + 1 >= value.size()) {
            output += value[index];
            continue;
        }

        const char escaped = value[++index];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            output += escaped;
            break;
        case 'b':
            output += '\b';
            break;
        case 'f':
            output += '\f';
            break;
        case 'n':
            output += '\n';
            break;
        case 'r':
            output += '\r';
            break;
        case 't':
            output += '\t';
            break;
        case 'u': {
            if (index + 4 >= value.size()) {
                break;
            }
            unsigned int codepoint = 0;
            bool valid = true;
            for (int offset = 0; offset < 4; ++offset) {
                const int digit = hexValue(value[index + 1 + offset]);
                if (digit < 0) {
                    valid = false;
                    break;
                }
                codepoint = (codepoint << 4) | static_cast<unsigned int>(digit);
            }
            if (valid) {
                appendUtf8(output, codepoint);
                index += 4;
            }
            break;
        }
        default:
            output += escaped;
            break;
        }
    }
    return output;
}

bool extractTranslatedText(const std::string &response, std::string &translation) {
    const std::string key = "\"translatedText\":\"";
    const size_t start = response.find(key);
    if (start == std::string::npos) {
        return false;
    }

    std::string raw;
    bool escaping = false;
    for (size_t index = start + key.size(); index < response.size(); ++index) {
        const char value = response[index];
        if (!escaping && value == '"') {
            translation = unescapeJsonString(raw);
            return !translation.empty();
        }
        raw += value;
        escaping = !escaping && value == '\\';
        if (value != '\\') {
            escaping = false;
        }
    }
    return false;
}

} // namespace

bool translateFrenchToDutch(const std::string &text, std::string &translation, std::string &error) {
    translation.clear();
    error.clear();

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        error = "Could not initialize curl";
        return false;
    }

    char *escaped = curl_easy_escape(curl, text.c_str(), static_cast<int>(text.size()));
    if (escaped == nullptr) {
        curl_easy_cleanup(curl);
        error = "Could not encode text";
        return false;
    }

    const std::string url = std::string("https://api.mymemory.translated.net/get?q=") + escaped + "&langpair=fr%7Cnl";
    curl_free(escaped);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NXReader/0.1");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 14L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        curl_easy_cleanup(curl);
        return false;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (status < 200 || status >= 300) {
        char buffer[64] = {};
        std::snprintf(buffer, sizeof(buffer), "HTTP %ld", status);
        error = buffer;
        return false;
    }

    if (!extractTranslatedText(response, translation)) {
        error = "Translation not found in response";
        return false;
    }

    return true;
}

} // namespace nxreader
