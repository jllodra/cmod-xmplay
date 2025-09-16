// http_head.hpp
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <ctime>
#pragma comment(lib, "winhttp.lib")

inline bool HttpHead_LastModified(
    const wchar_t* url,
    time_t* lastModifiedUtc,                   // out (nullable)
    unsigned long long* contentLength = nullptr,     // out (nullable)
    std::wstring* etag = nullptr                    // out (nullable)
) {
    if (lastModifiedUtc) *lastModifiedUtc = 0;
    if (contentLength)   *contentLength = 0;
    if (etag)            etag->clear();

    // Crack URL
    URL_COMPONENTSW uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256];      uc.lpszHostName = host;  uc.dwHostNameLength = _countof(host);
    wchar_t path[2048];     uc.lpszUrlPath = path;  uc.dwUrlPathLength = _countof(path);
    wchar_t extra[1024];    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = _countof(extra);
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return false;

    std::wstring hostW(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring pathW(uc.lpszUrlPath, uc.dwUrlPathLength);
    pathW.append(uc.lpszExtraInfo, uc.dwExtraInfoLength); // include query string, etc.

    // Open session/connection/request
    HINTERNET hSession = WinHttpOpen(L"cmod/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, hostW.c_str(), uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"HEAD", pathW.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    // Follow redirects automatically
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

    // Send + receive
    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    // Status code 200 expected here
    DWORD status = 0, cb = sizeof(status);
    if (WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &cb, WINHTTP_NO_HEADER_INDEX)) {
        if (status != 200) {
            WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            return false;
        }
    }

    // Last-Modified -> time_t (UTC)
    if (lastModifiedUtc) {
        SYSTEMTIME st{}; DWORD stLen = sizeof(st);
        if (WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_LAST_MODIFIED | WINHTTP_QUERY_FLAG_SYSTEMTIME,
            WINHTTP_HEADER_NAME_BY_INDEX, &st, &stLen, WINHTTP_NO_HEADER_INDEX)) {
            FILETIME ft{};
            if (SystemTimeToFileTime(&st, &ft)) {
                ULARGE_INTEGER uli;
                uli.LowPart = ft.dwLowDateTime;
                uli.HighPart = ft.dwHighDateTime;
                const ULONGLONG EPOCH_DIFF = 116444736000000000ULL; // 1601->1970 (100ns)
                if (uli.QuadPart >= EPOCH_DIFF) {
                    *lastModifiedUtc = (time_t)((uli.QuadPart - EPOCH_DIFF) / 10000000ULL);
                }
            }
        }
    }

    // Content-Length (optional)
    if (contentLength) {
        wchar_t buf[64]; DWORD len = sizeof(buf);
        if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX, buf, &len, WINHTTP_NO_HEADER_INDEX)) {
            // len is in bytes, may not be null-terminated
            size_t wchar_count = len / sizeof(wchar_t);
            if (wchar_count > 0 && buf[wchar_count - 1] == L'\0') {
                --wchar_count; // Exclude null terminator if present
            }
            std::wstring contentLengthStr(buf, wchar_count);
            *contentLength = _wcstoui64(contentLengthStr.c_str(), nullptr, 10);
        }
    }

    // ETag (optional)
    if (etag) {
        wchar_t buf[256]; DWORD len = sizeof(buf);
        if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_ETAG,
            WINHTTP_HEADER_NAME_BY_INDEX, buf, &len, WINHTTP_NO_HEADER_INDEX)) {
            size_t wchar_count = len / sizeof(wchar_t);
            if (wchar_count > 0 && buf[wchar_count - 1] == L'\0') {
                --wchar_count; // Exclude null terminator if present
            }
            *etag = std::wstring(buf, wchar_count);
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}
