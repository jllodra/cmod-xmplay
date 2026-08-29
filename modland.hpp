#pragma once
#include <string>
#include "utf.hpp"

inline bool is_unreserved_or_slash(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
}

std::string url_encode_utf8(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (unsigned char c : input) {
        if (is_unreserved_or_slash(c)) {
            out.push_back((char)c);
        }
        else {
            char buf[4];
            _snprintf_s(buf, _TRUNCATE, "%%%02X", c);
            out.append(buf);
        }
    }
    return out;
}

inline const char* default_modland_base_url() {
    return "http://modland.com";
}

inline std::string& modland_base_url_storage() {
    static std::string baseUrl = default_modland_base_url();
    return baseUrl;
}

inline void set_modland_base_url(const std::string& baseUrl) {
    modland_base_url_storage() = baseUrl;
}

inline const std::string& modland_base_url() {
    return modland_base_url_storage();
}

inline std::string modland_allmods_url() {
    return modland_base_url() + "/allmods.zip";
}

inline std::string modland_url_from_pathU8(const std::string& pathUtf8) {
    return modland_base_url() + "/pub/modules/" + url_encode_utf8(pathUtf8);
}

inline std::string modland_url_from_pathW(const std::wstring& pathW) {
    std::string u8 = to_utf8(pathW);
    return modland_url_from_pathU8(u8);
}
