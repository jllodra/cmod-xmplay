#pragma once
#include <string>
#include <Shlwapi.h>
#include "utf.hpp"
#pragma comment(lib, "Shlwapi.lib")

std::string url_encode_utf8(const std::string& input) {
    // 0) First convert all '#' → "%23"
    std::string pre;
    pre.reserve(input.size());
    for (unsigned char c : input) {
        if (c == '#') pre += "%23";
        else          pre += c;
    }

    // 1) Ask UrlEscapeA how big the buffer must be (including NUL)
    DWORD needed = 0;
    UrlEscapeA(
        pre.c_str(),
        nullptr,
        &needed,
        URL_ESCAPE_PERCENT
    );
    if (needed == 0)
        return pre;  // nothing to escape (or error)

    // 2) Do the real escape
    std::string out;
    out.resize(needed);
    if (SUCCEEDED(UrlEscapeA(
        pre.c_str(),
        &out[0],
        &needed,
        URL_ESCAPE_PERCENT)))
    {
        // trim trailing NUL
        out.resize(needed - 1);
        return out;
    }

    // fallback
    return pre;
}

inline std::string modland_url_from_pathU8(const std::string& pathUtf8) {
    return "https://modland.com/pub/modules/" + url_encode_utf8(pathUtf8);
}

inline std::string modland_url_from_pathW(const std::wstring& pathW) {
    std::string u8 = to_utf8(pathW);
    return modland_url_from_pathU8(u8);
}