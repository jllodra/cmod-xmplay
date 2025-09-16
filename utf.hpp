// utf.hpp (o sección utils en tu .cpp)
#pragma once
#include <string>
#include <windows.h>
#include "sqlite\sqlite3.h"

inline std::string to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n - 1, '\0'); // sin NUL
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

inline std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n - 1, L'\0'); // sin NUL
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

inline std::wstring wide_from_sqlite(sqlite3_stmt* st, int col) {
    // Copia segura de la cadena UTF-16 que devuelve SQLite (puede incluir BOM)
    const wchar_t* raw = reinterpret_cast<const wchar_t*>(sqlite3_column_text16(st, col));
    if (!raw) return {};
    if (raw[0] == 0xFEFF || raw[0] == 0xFFFE) ++raw; // quita BOM si la hay
    return std::wstring(raw);
}

inline std::wstring get_dlg_textW(HWND hDlg, int ctlId) {
    wchar_t tmp[512];
    GetDlgItemTextW(hDlg, ctlId, tmp, _countof(tmp));
    return std::wstring(tmp);
}

inline void set_dlg_textW(HWND hDlg, int ctlId, const std::wstring& w) {
    SetDlgItemTextW(hDlg, ctlId, w.c_str());
}
