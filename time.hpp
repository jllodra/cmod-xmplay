#include <string>
#include <ctime>
#include <fileapi.h>

static std::string to_iso8601_utc(time_t t)
{
    std::tm tm{};
    gmtime_s(&tm, &t);                       // UTC
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

static time_t file_mtime_utc(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;

    ULARGE_INTEGER ull{};
    ull.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    // FILETIME (100ns desde 1601) → Unix epoch (s desde 1970)
    const unsigned long long EPOCH_DIFF = 116444736000000000ULL;
    if (ull.QuadPart < EPOCH_DIFF) return 0;
    return (time_t)((ull.QuadPart - EPOCH_DIFF) / 10000000ULL);
}

static std::wstring ymd_local(time_t t)
{
    if (!t) return L"(none)";
    std::tm lt{};
    localtime_s(&lt, &t);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    return buf;
}