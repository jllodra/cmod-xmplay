// pls.hpp - header-only helpers to build and feed temporary PLS playlists to XMPlay
#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <shlwapi.h>   // PathFindExtensionW, PathRenameExtensionW
#include "utf.hpp"
#include <CommCtrl.h>
#include "modland.hpp"
#pragma comment(lib, "Shlwapi.lib")

inline std::wstring StripExt(const std::wstring& s) {
    const wchar_t* p = PathFindExtensionW(s.c_str());
    if (p && *p == L'.') return std::wstring(s.c_str(), p - s.c_str());
    return s;
}

inline std::wstring GetPluginDirW(HINSTANCE hInstance) {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(hInstance, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    return modulePath;
}

// Encode Windows path to a file:/// URL (UTF-8 + percent-encode minimal set)
inline std::string FileUrlFromPathW(const std::wstring& pathW) {
    wchar_t full[MAX_PATH];
    GetFullPathNameW(pathW.c_str(), MAX_PATH, full, nullptr);
    std::string u8 = to_utf8(full);

    std::string out; out.reserve(u8.size() + 16);
    out += "file:///";
    for (unsigned char c : u8) {
        // keep safe URL chars; convert backslashes to slashes
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == ':' || c == '/' || c == '\\' ||
            c == '.' || c == '-' || c == '_')
        {
            out.push_back(c == '\\' ? '/' : c);
        }
        else {
            char buf[4];
            _snprintf_s(buf, _TRUNCATE, "%%%02X", c);
            out.append(buf);
        }
    }
    return out;
}

struct PlsEntry {
    std::string fileUrl;  // URL on the configured Modland mirror
    std::string title;    // e.g., "Artist - Song"
};

// Writes UTF-8 BOM + CRLF PLSv2 to a stable path %TEMP%\cmod_xmplay.pls
// using an atomic replace. Returns the stable path and file:// URL.
inline bool WriteTempPls(const std::vector<PlsEntry>& items,
    std::wstring* outPathW,
    std::string* outFileUrl)
{
    if (items.empty()) return false;

    wchar_t tmpDir[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, tmpDir)) return false;

    // Stable target + staging temp
    std::wstring stable = tmpDir;
    stable += L"cmodtmp.pls";
    std::wstring staging = tmpDir;
    staging += L"cmodtmp.tmp";

    // 1) Write to staging
    HANDLE h = CreateFileW(staging.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    auto write = [&](const std::string& s) {
        DWORD wr = 0; return WriteFile(h, s.data(), (DWORD)s.size(), &wr, nullptr) && (wr == s.size());
    };

    // UTF-8 BOM + header
    if (!write(std::string("\xEF\xBB\xBF", 3)) || !write("[playlist]\r\n")) {
        CloseHandle(h); DeleteFileW(staging.c_str()); return false;
    }

    for (size_t i = 0; i < items.size(); ++i) {
        char n[32]; _snprintf_s(n, _TRUNCATE, "%zu", i + 1);
        if (!write(std::string("File") + n + "=" + items[i].fileUrl + "\r\n")) { CloseHandle(h); DeleteFileW(staging.c_str()); return false; }
        if (!write(std::string("Title") + n + "=" + items[i].title + "\r\n")) { CloseHandle(h); DeleteFileW(staging.c_str()); return false; }
        if (!write(std::string("Length") + n + "=-1\r\n")) { CloseHandle(h); DeleteFileW(staging.c_str()); return false; }
    }
    char count[32]; _snprintf_s(count, _TRUNCATE, "%zu", items.size());
    if (!write(std::string("NumberOfEntries=") + count + "\r\nVersion=2\r\n")) {
        CloseHandle(h); DeleteFileW(staging.c_str()); return false;
    }
    CloseHandle(h);

    // 2) Atomically replace stable with staging (handle sharing violations)
    for (int attempts = 0; attempts < 5; ++attempts) {
        if (MoveFileExW(staging.c_str(), stable.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            if (outPathW)  *outPathW = stable;
            if (outFileUrl)*outFileUrl = FileUrlFromPathW(stable);
            return true;
        }
        DWORD err = GetLastError();
        if (err != ERROR_SHARING_VIOLATION && err != ERROR_ACCESS_DENIED) break;
        Sleep(10); // brief backoff if XMPlay still has it open
    }

    // If replace failed, clean up staging
    DeleteFileW(staging.c_str());
    return false;
}

// Build entries from your ListView row (columns: 1=Artist, 2=Song, 4=Full Path)
inline void CollectPlsEntriesFromList(HWND hList,
    bool onlySelected,
    std::vector<PlsEntry>& out)
{
    const int total = ListView_GetItemCount(hList);
    auto addRow = [&](int row) {
        WCHAR wArtist[MAX_PATH] = L"", wSong[MAX_PATH] = L"", wPath[MAX_PATH] = L"";
        ListView_GetItemText(hList, row, 1, wArtist, _countof(wArtist)); // Artist
        ListView_GetItemText(hList, row, 2, wSong, _countof(wSong));   // Song (filename)
        ListView_GetItemText(hList, row, 4, wPath, _countof(wPath));   // Full Path

        // Title = "Artist - Song" (strip file extension)
        const wchar_t* SEP = L" \u2014 ";

        std::wstring niceSong = StripExt(wSong);
        std::wstring titleW = std::wstring(wArtist) + SEP + (niceSong.empty() ? wSong : niceSong);

        PlsEntry e;
        e.fileUrl = modland_url_from_pathW(wPath);  // you already have this
        e.title = to_utf8(titleW);
        out.push_back(std::move(e));
    };

    if (onlySelected) {
        int idx = -1;
        while ((idx = ListView_GetNextItem(hList, idx, LVNI_SELECTED)) != -1)
            addRow(idx);
    }
    else {
        for (int i = 0; i < total; ++i) addRow(i);
    }
}
