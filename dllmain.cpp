#include "pch.h"

#include <process.h>
#include <string>
#include <cstdlib>
#include <locale>     // std::locale
#include <codecvt>    // std::codecvt_utf8<wchar_t>
#include <windows.h>
#include <cwchar>      // for std::wcstoll
#include <fstream>
#include <sstream> 
#include <commctrl.h>
#include <urlmon.h>
#include <math.h>
#include "miniz/miniz.h"   // from miniz.c/.h
#include "xmpfunc.h"
#include "xmpdsp.h"
#include "resource.h"  // define IDD_SEARCH
#include "sqlite\sqlite3.h"
#include <Shlwapi.h>    // PathRemoveFileSpecW
#include <map>
#include <set>
#include <vector>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "urlmon.lib")

static InterfaceProc g_faceproc;
static XMPFUNC_MISC* xmpfmisc;
static XMPFUNC_REGISTRY* xmpfreg;

static void *WINAPI     Plugin_Init(void);
static void WINAPI     Plugin_Exit(void* inst);
static const char* WINAPI Plugin_GetDescription(void* inst);
static void WINAPI     MyConfig(void* inst, HWND win);
static DWORD WINAPI    Plugin_GetConfig(void* inst, void* config);
static BOOL WINAPI     Plugin_SetConfig(void* inst, void* config, DWORD size);
static BOOL CALLBACK   SearchDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
static void WINAPI     OpenSearchShortcut(void);
static void DoSearch(HWND hDlg);

struct PluginConfig {
};

struct PluginData {
    int x;
};

static PluginData* pluginData;

static HWND hSearchDlg = nullptr;

static HINSTANCE hInstance;
static HWND hWndConf = 0;

static HWND hWndXMP;

static sqlite3* g_db = nullptr;

static int g_colInitWidth[4] = { 0, 0, 0, 0 };

#define WM_DB_REBUILT    (WM_APP + 100)

struct RebuildParams {
    HWND    hDlg;
    HINSTANCE hInst;
    XMPFUNC_MISC* xmpfmisc;
};

static const char* g_colNames[] = {
    "extension",  // columna 0
    "artist",     // columna 1
    "song",       // columna 2
    "full_path"   // columna 3
};
static int  g_sortColumn = -1;   // -1 = sin ordenar todavía
static bool g_sortAsc = true; // sentido actual

struct SortData {
    HWND  hList;
    int   column;
    bool  asc;
};

struct CtrlInfo {
    RECT  rc;        // rect original en coords de cliente
    bool  moveX;     // si debe moverse horizontalmente
    bool  moveY;     // si debe moverse verticalmente
    bool  sizeW;     // si debe cambiar de ancho
    bool  sizeH;     // si debe cambiar de alto
};
static RECT               g_rcInitClient;
static std::map<int, CtrlInfo> g_mapCtrls;

// -- DB rebuild

static bool DownloadAllmods(const std::wstring& url, const std::wstring& destZip) {
    return SUCCEEDED(URLDownloadToFileW(NULL, url.c_str(),
        destZip.c_str(), 0, NULL));
}

static std::string ws2utf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int len = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
        nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s;
    s.resize(len);
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
        &s[0], len, nullptr, nullptr);
    return s;
}

static bool ExtractAllmodsTxt(const std::wstring& zipPathW,
    const std::wstring& outTxtPathW)
{
    // 1) UTF-8 paths
    std::string zipPath = ws2utf8(zipPathW);
    std::string outTxt = ws2utf8(outTxtPathW);

    // 2) Extract into a heap buffer in one call
    size_t uncompressed_size = 0;
    void* p = mz_zip_extract_archive_file_to_heap(
        zipPath.c_str(),     // path to .zip on disk
        "allmods.txt",       // the file inside
        &uncompressed_size,  // filled with its size
        0                     // flags (0 = default)
    );
    if (!p) return false;

    // 3) Write that buffer out to your .txt
    FILE* fp = nullptr;
    if (fopen_s(&fp, outTxt.c_str(), "wb") != 0 || !fp) {
        mz_free(p);
        return false;
    }
    fwrite(p, 1, uncompressed_size, fp);
    fclose(fp);

    // 4) Clean up
    mz_free(p);
    return true;
}

static bool RebuildDatabase(const std::wstring& txtPathW,
    const std::wstring& dbPathW)
{
    // Remove any old DB
    ::DeleteFileW(dbPathW.c_str());

    // Open new DB
    sqlite3* db = nullptr;
    if (sqlite3_open16(dbPathW.c_str(), &db) != SQLITE_OK)
        return false;

    // Speed tweaks: in-memory journal + no fsync
    sqlite3_exec(db, "PRAGMA journal_mode = MEMORY;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous = OFF;", nullptr, nullptr, nullptr);

    // Create FTS5 table
    const char* ddl =
        "CREATE VIRTUAL TABLE modland USING fts5("
        " tracker, extension, artist, song, full_path, size UNINDEXED"
        ");";
    if (sqlite3_exec(db, ddl, nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    // Begin one big transaction
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // Prepare insert statement once
    sqlite3_stmt* ins = nullptr;
    const char* sql = "INSERT INTO modland VALUES (?1,?2,?3,?4,?5,?6);";
    if (sqlite3_prepare_v2(db, sql, -1, &ins, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    // Allowed extensions
    static const std::set<std::string> allowed = {
        "mod","s3m","xm","it","mo3","mtm","umx"
    };

    // Read & parse line-by-line
    std::wifstream in(txtPathW);
    in.imbue(std::locale(in.getloc(), new std::codecvt_utf8<wchar_t>));
    std::wstring line;
    while (std::getline(in, line)) {
        // split off size
        auto tab = line.find(L'\t');
        if (tab == std::wstring::npos) continue;
        std::wstring wsiz = line.substr(0, tab);
        std::wstring rest = line.substr(tab + 1);

        // extension
        auto dot = rest.rfind(L'.');
        if (dot == std::wstring::npos) continue;
        std::string ext = ws2utf8(rest.substr(dot + 1));
        if (!allowed.count(ext)) continue;

        // first/last slash
        auto firstSlash = rest.find(L'/');
        auto lastSlash = rest.rfind(L'/');
        if (firstSlash == std::wstring::npos || lastSlash == firstSlash)
            continue;

        std::wstring tracker = rest.substr(0, firstSlash);
        std::wstring artist = rest.substr(firstSlash + 1,
            lastSlash - firstSlash - 1);
        std::wstring song = rest.substr(lastSlash + 1);

        // Bind UTF-8 params
        sqlite3_bind_text(ins, 1, ws2utf8(tracker).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 2, ext.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 3, ws2utf8(artist).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 4, ws2utf8(song).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(ins, 5, ws2utf8(rest).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(ins, 6, std::wcstoll(wsiz.c_str(), nullptr, 10));

        sqlite3_step(ins);
        sqlite3_reset(ins);
    }

    // Finish up
    sqlite3_finalize(ins);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);


    return true;
}

// Closes the old g_db, renames cmod_new.db → cmod.db, and re-opens g_db
static bool SwapInNewDatabase()
{
    // 1) Compute paths
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(hInstance, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    std::wstring dir = modulePath;

    std::wstring oldDb = dir + L"\\cmod.db";
    std::wstring newDb = dir + L"\\cmod_new.db";
    std::wstring zip = dir + L"\\allmods.zip";
    std::wstring txt = dir + L"\\allmods.txt";

    // 2) Close old handle
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }

    // 3) Replace files on disk
    ::DeleteFileW(oldDb.c_str());
    if (!::MoveFileW(newDb.c_str(), oldDb.c_str()))
        return false;

    // 4) Re-open into g_db
    if (sqlite3_open16(oldDb.c_str(), &g_db) != SQLITE_OK) {
        g_db = nullptr;
        return false;
    }

    ::DeleteFileW(zip.c_str());
    ::DeleteFileW(txt.c_str());

    return true;
}

static unsigned __stdcall RebuildThread(void* pv)
{
    auto* p = (RebuildParams*)pv;
    HWND hDlg = p->hDlg;

    // 1) Download
    xmpfmisc->ShowBubble("Downloading allmods…", 1000);
    if (!DownloadAllmods(L"https://modland.com/allmods.zip", L"allmods.zip"))
        goto fail;

    // 2) Unzip
    xmpfmisc->ShowBubble("Extracting…", 1000);
    if (!ExtractAllmodsTxt(L"allmods.zip", L"allmods.txt"))
        goto fail;

    // 3) Rebuild
    xmpfmisc->ShowBubble("Rebuilding DB…", 1000);
    if (!RebuildDatabase(L"allmods.txt", L"cmod_new.db"))
        goto fail;

    // 4) Swap new → active cmod.db
    xmpfmisc->ShowBubble("Swapping in new DB…", 1000);
    if (!SwapInNewDatabase())
        goto fail;

    // Success
    PostMessageW(hDlg, WM_DB_REBUILT, TRUE, 0);
    delete p;
    return 0;

fail:
    PostMessageW(hDlg, WM_DB_REBUILT, FALSE, 0);
    delete p;
    return 0;
}

static bool RecreateDatabaseNow()
{
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(hInstance, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    std::wstring dir = modulePath;

    std::wstring zip = dir + L"\\allmods.zip";
    std::wstring txt = dir + L"\\allmods.txt";
    std::wstring newDb = dir + L"\\cmod_new.db";

    // 1) Download
    if (!DownloadAllmods(L"https://modland.com/allmods.zip", zip))
        return false;

    // 2) Extract
    if (!ExtractAllmodsTxt(zip, txt))
        return false;

    // 3) Rebuild into cmod_new.db
    if (!RebuildDatabase(txt, newDb))
        return false;

    // 4) Swap in and open
    if (!SwapInNewDatabase())
        return false;

    return true;
}

static unsigned __stdcall StartupRebuildThread(void* /*unused*/)
{
    xmpfmisc->ShowBubble("Recreating cmod.db…", 2000);
    if (RecreateDatabaseNow()) {
        xmpfmisc->ShowBubble("cmod.db ready!", 1000);
    }
    else {
        xmpfmisc->ShowBubble("Failed to recreate cmod.db", 2000);
    }
    return 0;
}

// ---- Implementación ----

static void *WINAPI Plugin_Init(void) {
    // Obtener APIs
    xmpfmisc = (XMPFUNC_MISC*)g_faceproc(XMPFUNC_MISC_FACE);
    xmpfreg = (XMPFUNC_REGISTRY*)g_faceproc(XMPFUNC_REGISTRY_FACE);

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(hInstance, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    std::wstring dbPath = std::wstring(modulePath) + L"\\cmod.db";

    DWORD attrs = GetFileAttributesW(dbPath.c_str());
    bool opened = false;

    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        // missing on disk
        xmpfmisc->ShowBubble("cmod.db not found, recreating…", 2000);
    }
    else {
        // try to open existing
        if (sqlite3_open16(dbPath.c_str(), &g_db) == SQLITE_OK) {
            opened = true;
        }
    }

    if (!opened) {
        // don’t block—fire off the rebuild in the background
        xmpfmisc->ShowBubble("cmod.db missing or corrupt, rebuilding…", 2000);
        uintptr_t th = _beginthreadex(
            nullptr, 0,
            StartupRebuildThread,
            nullptr, 0, nullptr
        );
        if (th) CloseHandle((HANDLE)th);
    }

    // Registrar diálogo de configuración como menú DSP
    pluginData = (PluginData*)calloc(1, sizeof(*pluginData));
    return pluginData;
}

static void WINAPI Plugin_Exit(void* inst) {
    // Cerrar BD, detener hilos, etc.
    if (g_db) {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

static const char* WINAPI Plugin_GetDescription(void* inst) {
    return "cmod (modland) by herotyc";
}

static void WINAPI MyConfig(void* inst, HWND win) {
    /*DialogBoxParam(
        (HINSTANCE)g_faceproc(XMPFUNC_WINDOW_FACE),
        MAKEINTRESOURCE(IDD_SEARCH),
        win,
        SearchDlgProc,
        0
    );*/
}

static DWORD WINAPI Plugin_GetConfig(void* inst, void* config) {
    // Tamaño de PluginConfig
    // if (config) memcpy(config, &(PluginConfig){}, sizeof(PluginConfig));
    return sizeof(PluginConfig);
}

static BOOL WINAPI Plugin_SetConfig(void* inst, void* config, DWORD size) {
    // Asignar config si guardas algo
    return TRUE;
}

static LRESULT CALLBACK EditSubclassProc(
    HWND    hWnd,
    UINT    msg,
    WPARAM  wParam,
    LPARAM  lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData
) {

    if (msg == WM_GETDLGCODE) {
        // Sólo interesa interceptar RETURN, no todas las teclas
        return DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        DoSearch(GetParent(hWnd));
        return 0;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static void DoSearch(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
    ListView_DeleteAllItems(hList);

    // 1) Leer búsqueda (UTF-16) y convertir a UTF-8 + wildcard
    wchar_t wbuf[256];
    GetDlgItemTextW(hDlg, IDC_EDIT_SEARCH, wbuf, _countof(wbuf));

    char buf[256];
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, buf, sizeof(buf), nullptr, nullptr);
    std::string query = std::string(buf) + "*";

    // 2) Determinar columna a usar
    HWND hComboSearch = GetDlgItem(hDlg, IDC_COMBO_SEARCH);
    int sel = (int)SendMessageW(hComboSearch, CB_GETCURSEL, 0, 0);
    const char* column;
    switch (sel) {
        case 0:  column = "artist"; break;
        case 1:  column = "song"; break;
        case 2:  column = "modland";   break;
        default: column = "modland"; // All: toda la tabla FTS5
    }

    HWND hComboFormat = GetDlgItem(hDlg, IDC_COMBO_FORMAT);
    sel = (int)SendMessageW(hComboFormat, CB_GETCURSEL, 0, 0);
    const char* format;
    switch (sel) {
        case 0:  format = "any"; break;
        case 1:  format = "it"; break;
        case 2:  format = "xm";   break;
        case 3:  format = "s3m"; break;
        case 4:  format = "mod";   break;
        default: format = "any"; // All: toda la tabla FTS5
    }

    // 2) Preparar la consulta FTS5 con ORDER BY artist, bm25
    std::string orderBy;

    // ¿Hay una columna activa para ordenar?
    if (g_sortColumn >= 0 && g_sortColumn < 4) {
        // p.ej. "artist COLLATE NOCASE DESC"
        orderBy = std::string(g_colNames[g_sortColumn])
            + " COLLATE NOCASE "
            + (g_sortAsc ? "ASC" : "DESC")
            + ", bm25(modland) ASC";
    }
    else {
        // default si no se ha pinchado ninguna columna
        orderBy = "artist COLLATE NOCASE ASC, bm25(modland) ASC";
    }

    // Monta el SQL con ORDER BY dinámico
    std::string sql =
        "SELECT upper(extension), artist, song, full_path "
        "FROM modland "
        "WHERE " + std::string(column) + " MATCH ?1 ";

    if (std::string(format) != "any") {
        sql += "AND extension = ?2 ";
    }


    sql +=
        "ORDER BY " + orderBy + " "
        "LIMIT 1000;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        MessageBoxA(NULL, sql.c_str(), "SQLite error", MB_OK | MB_ICONERROR);
        MessageBoxA(NULL, sqlite3_errmsg(g_db), "SQLite error", MB_OK | MB_ICONERROR);
        return;
    }

    // 3) Binder parámetro
    sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);

    if (std::string(format) != "any") {
        sqlite3_bind_text(stmt, 2, format, -1, SQLITE_TRANSIENT);
    }

    // 4) Recorrer resultados e insertarlos en el ListView
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // UTF-8 desde SQLite
        const char* ext = (const char*)sqlite3_column_text(stmt, 0);
        const char* artist = (const char*)sqlite3_column_text(stmt, 1);
        const char* song = (const char*)sqlite3_column_text(stmt, 2);
        const char* path = (const char*)sqlite3_column_text(stmt, 3);

        // Convertir a UTF-16 para el ListViewW
        wchar_t wExt[128], wArtist[128], wSong[128], wPath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, ext, -1, wExt, _countof(wExt));
        MultiByteToWideChar(CP_UTF8, 0, artist, -1, wArtist, _countof(wArtist));
        MultiByteToWideChar(CP_UTF8, 0, song, -1, wSong, _countof(wSong));
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, _countof(wPath));

        // 4.1) Insertar columna 0 (extension)
        LVITEMW item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.iSubItem = 0;
        item.pszText = wExt;
        ListView_InsertItem(hList, &item);

        // 4.2) Rellenar subitems 1..3
        ListView_SetItemText(hList, index, 1, wArtist);
        ListView_SetItemText(hList, index, 2, wSong);
        ListView_SetItemText(hList, index, 3, wPath);

        ++index;
    }

    sqlite3_finalize(stmt);

    int count = ListView_GetItemCount(hList);
    // 1) Construye un std::wstring que contenga tu texto
    std::wstring txt =
        std::to_wstring(count)
        + (count == 1 ? L" result" : L" results");

    // 2) Y pásalo a SetDlgItemTextW, que espera LPCWSTR
    SetDlgItemTextW(hDlg, IDC_STATIC_COUNT, txt.c_str());
}

static int CALLBACK ListCompare(LPARAM l1, LPARAM l2, LPARAM lp)
{
    const SortData* sd = reinterpret_cast<SortData*>(lp);
    WCHAR text1[512]{}, text2[512]{};

    ListView_GetItemText(sd->hList, (int)l1, sd->column, text1, _countof(text1));
    ListView_GetItemText(sd->hList, (int)l2, sd->column, text2, _countof(text2));

    int cmp = _wcsicmp(text1, text2);      // comparación UTF-16 “case-insensitive”
    if (!sd->asc) cmp = -cmp;              // invertir si descendente
    return cmp;
}

static BOOL CALLBACK SearchDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
        {
            // 1) Captura tamaño cliente inicial
            GetClientRect(hDlg, &g_rcInitClient);

            // 2) Define qué controles y cómo se anclan/expanden
            struct Def { int id; bool mX, mY, sW, sH; };
            Def a[] = {
                { IDC_LIST_RESULTS, false,  false,  true,  true },   // lista se expande en todo
                { IDC_EDIT_SEARCH,  false,false, true,  false},   // edit solo crece en ancho
                { IDC_COMBO_SEARCH, true,  false, false, false},  // comboSearch se mueve en X
                { IDC_STATIC_SEARCHBY,true,false,false, false},   // idem label
                { IDC_COMBO_FORMAT,true,  false, false, false},   // comboFormat se mueve en X
                { IDC_STATIC_FORMAT,true,false,false, false},     // label format
                { IDC_BUTTON_ADD_ALL,false,true, false, false},   // botón ADD se mueve en Y
                { IDC_BUTTON_REBUILD, true, true, false, false},   // botón ADD se mueve en Y
                { IDC_STATIC_COUNT, false,  true,  false, false},  // contador se mueve X e Y
                { IDCANCEL,         true,  true,  false, false}   // botón Close idem
            };

            for (auto& d : a) {
                HWND h = GetDlgItem(hDlg, d.id);
                RECT r;
                GetWindowRect(h, &r);
                ScreenToClient(hDlg, (LPPOINT)&r);
                ScreenToClient(hDlg, ((LPPOINT)&r) + 1);
                g_mapCtrls[d.id] = { r, d.mX, d.mY, d.sW, d.sH };
            }

            // 1) Inicializar ListView
            INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
            InitCommonControlsEx(&icex);

            SetDlgItemTextW(hDlg, IDC_STATIC_COUNT, L"0 results");

            HWND hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
            //--- estilos extendidos ---
            DWORD ex = ListView_GetExtendedListViewStyle(hList);
            ex |= LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
            ListView_SetExtendedListViewStyle(hList, ex);

            LVCOLUMN col = { 0 };
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 40; col.pszText = const_cast<LPWSTR>(TEXT("Ext"));        ListView_InsertColumn(hList, 0, &col); g_colInitWidth[0] = col.cx;
            col.cx = 120; col.pszText = const_cast<LPWSTR>(TEXT("Artist"));    ListView_InsertColumn(hList, 1, &col); g_colInitWidth[1] = col.cx;
            col.cx = 140; col.pszText = const_cast<LPWSTR>(TEXT("Song"));      ListView_InsertColumn(hList, 2, &col); g_colInitWidth[2] = col.cx;
            col.cx = 300; col.pszText = const_cast<LPWSTR>(TEXT("Full Path")); ListView_InsertColumn(hList, 3, &col); g_colInitWidth[3] = col.cx;

            // 2) Subclasificar el EDIT para detectar Enter
            HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_SEARCH);
            SetWindowSubclass(hEdit, EditSubclassProc, 0, 0);
            SetFocus(hEdit);

            // 3) Inicializa el ComboBox de campo
            HWND hComboSearch = GetDlgItem(hDlg, IDC_COMBO_SEARCH);
            SendMessageW(hComboSearch, CB_RESETCONTENT, 0, 0);
            SendMessageW(hComboSearch, CB_ADDSTRING, 0, (LPARAM)L"Artist");
            SendMessageW(hComboSearch, CB_ADDSTRING, 0, (LPARAM)L"Song");
            SendMessageW(hComboSearch, CB_ADDSTRING, 0, (LPARAM)L"All");
            SendMessageW(hComboSearch, CB_SETCURSEL, 0, 0);  // por defecto "Artist"

            // 3) Inicializa el ComboBox de campo
            HWND hComboFormat = GetDlgItem(hDlg, IDC_COMBO_FORMAT);
            SendMessageW(hComboFormat, CB_RESETCONTENT, 0, 0);
            SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"Any");
            SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"IT");
            SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"XM");
            SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"S3M");
            SendMessageW(hComboFormat, CB_ADDSTRING, 0, (LPARAM)L"MOD");
            SendMessageW(hComboFormat, CB_SETCURSEL, 0, 0);  // por defecto "Any"

            // Load our plugin’s icon from resources
            HICON hIconSmall = (HICON)LoadImageW(
                hInstance,
                MAKEINTRESOURCE(IDI_ICON1),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON),
                GetSystemMetrics(SM_CYSMICON),
                LR_DEFAULTCOLOR
            );
            HICON hIconBig = (HICON)LoadImageW(
                hInstance,
                MAKEINTRESOURCE(IDI_ICON1),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXICON),
                GetSystemMetrics(SM_CYICON),
                LR_DEFAULTCOLOR
            );
            if (hIconSmall)
                SendMessageW(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
            if (hIconBig)
                SendMessageW(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);


            return TRUE;
        }

        case WM_GETMINMAXINFO: {
            // lParam apunta a un MINMAXINFO
            LPMINMAXINFO pMMI = (LPMINMAXINFO)lParam;
            pMMI->ptMinTrackSize.x = 500;
            pMMI->ptMinTrackSize.y = 400;
            return TRUE;  // hemos procesado el mensaje
        }

        case WM_SIZE:
        {
            int newW = LOWORD(lParam), newH = HIWORD(lParam);
            int dx = newW - g_rcInitClient.right;
            int dy = newH - g_rcInitClient.bottom;

            for (auto& kv : g_mapCtrls) {
                int id = kv.first;
                auto& ci = kv.second;
                int x = ci.rc.left + (ci.moveX ? dx : 0);
                int y = ci.rc.top + (ci.moveY ? dy : 0);
                int w = (ci.rc.right - ci.rc.left) + (ci.sizeW ? dx : 0);
                int h = (ci.rc.bottom - ci.rc.top) + (ci.sizeH ? dy : 0);
                MoveWindow(GetDlgItem(hDlg, id), x, y, w, h, TRUE);
            }

            if (dx != 0)
            {
                HWND hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
                // Calcula suma de anchos iniciales
                int sumInit = 0;
                for (int i = 0; i < 4; ++i) sumInit += g_colInitWidth[i];

                // Para cada columna, ajusta proporcionalmente:
                for (int i = 0; i < 4; ++i)
                {
                    // nuevo ancho = ancho_inicial + (ancho_inicial / suma_inicial) * dx
                    int w = g_colInitWidth[i] + MulDiv(g_colInitWidth[i], dx, sumInit);
                    ListView_SetColumnWidth(hList, i, w);
                }
            }
            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
            case IDC_BUTTON_ADD_ALL:
            {
                HWND hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
                int itemCount = ListView_GetItemCount(hList);
                for (int i = 0; i < itemCount; ++i) {
                    // 1) Obtener full_path (columna 3) en UTF-16
                    WCHAR wpath[MAX_PATH];
                    ListView_GetItemText(hList, i, 3, wpath, MAX_PATH);

                    // 2) Convertir a UTF-8
                    char pathUtf8[MAX_PATH];
                    WideCharToMultiByte(
                        CP_UTF8, 0,
                        wpath, -1,
                        pathUtf8, sizeof(pathUtf8),
                        nullptr, nullptr
                    );

                    // 3) Construir URL y comando DDE
                    std::string url = std::string("https://modland.com/pub/modules/") + pathUtf8;
                    std::string ddeCmd = "[list(" + url + ")]";

                    // 4) Enviar a XMPlay
                    xmpfmisc->DDE(ddeCmd.c_str());
                }
                return TRUE;
            }

            case IDC_BUTTON_REBUILD:
            {
                // disable button
                EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_REBUILD), FALSE);
                // launch thread
                auto* params = new RebuildParams{ hDlg };
                uintptr_t th = _beginthreadex(
                    nullptr, 0, RebuildThread, params, 0, nullptr
                );
                if (th) CloseHandle((HANDLE)th);
                else {
                    MessageBoxW(hDlg, L"Could not start rebuild", L"Error", MB_ICONERROR);
                    delete params;
                    EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_REBUILD), TRUE);
                }
                return TRUE;
            }

            case IDCANCEL:
                EndDialog(hDlg, 0);
                return TRUE;
            }
            break;
        }

        case WM_DB_REBUILT:
        {
            BOOL ok = (BOOL)wParam;
            xmpfmisc->ShowBubble(
                ok ? "DB rebuilt successfully!" : "DB rebuild failed.",
                1000
            );
            EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_REBUILD), TRUE);
            return TRUE;
        }

        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm->idFrom == IDC_LIST_RESULTS) {

                // --- 1) Custom-draw para sub-items sólo ---
                if (pnm->code == NM_CUSTOMDRAW) {
                    LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;

                    switch (cd->nmcd.dwDrawStage)
                    {
                    case CDDS_PREPAINT:
                        // Queremos notificación por ITEM
                        SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                        return TRUE;

                    case CDDS_ITEMPREPAINT:
                    {
                        // Si la fila está seleccionada, deja el color por defecto
                        if (cd->nmcd.uItemState & CDIS_SELECTED)
                            break;

                        // Texto de la 1.ª columna (Ext)
                        wchar_t ext[16] = L"";
                        ListView_GetItemText(
                            GetDlgItem(hDlg, IDC_LIST_RESULTS),
                            (int)cd->nmcd.dwItemSpec,   // índice de fila
                            0,                          // sub-ítem 0
                            ext, _countof(ext));

                        // Decide colores
                        if (_wcsicmp(ext, L"xm") == 0) {           // azul pastel
                            cd->clrTextBk = RGB(232, 235, 255);   //  ❮  antes 200,200,255
                            cd->clrText = RGB(16, 32, 128);   //  azul marino tenue
                        }
                        else if (_wcsicmp(ext, L"it") == 0) {           // naranja vainilla
                            cd->clrTextBk = RGB(255, 248, 228);   //  ❮  antes 255,240,200
                            cd->clrText = RGB(120, 72, 0);   //  marrón medio
                        }
                        else if (_wcsicmp(ext, L"s3m") == 0) {           // verde menta
                            cd->clrTextBk = RGB(225, 255, 225);   //  ❮  antes 200,255,200
                            cd->clrText = RGB(0, 104, 0);   //  verde oscuro
                        }
                        else if (_wcsicmp(ext, L"mod") == 0) {           // rosa claro
                            cd->clrTextBk = RGB(255, 236, 236);   //  ❮  antes 255,220,220
                            cd->clrText = RGB(136, 0, 48);   //  burdeos suave
                        }
                        // CDRF_NEWFONT para aplicar colores a toda la fila
                        SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_NEWFONT);
                        return TRUE;
                    }
                    } // switch(stage)

                    // Dejar que Windows haga lo demás
                    SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
                    return TRUE;
                }

                // 
                if (pnm->code == LVN_COLUMNCLICK)
                {
                    NMLISTVIEW* p = (NMLISTVIEW*)pnm;

                    // ¿misma columna? -> alternar asc/desc.  ¿nueva? -> asc por defecto
                    if (g_sortColumn == p->iSubItem)  g_sortAsc = !g_sortAsc;
                    else { g_sortColumn = p->iSubItem; g_sortAsc = true; }

                    // Construir datos y lanzar la ordenación
                    SortData sd{ GetDlgItem(hDlg, IDC_LIST_RESULTS), g_sortColumn, g_sortAsc };

                    ListView_SortItemsEx(sd.hList, ListCompare, (LPARAM)&sd);

                    // (Opcional) flecha en el encabezado
                    HWND hHeader = ListView_GetHeader(sd.hList);
                    int colCount = Header_GetItemCount(hHeader);
                    for (int i = 0; i < colCount; ++i)
                    {
                        HDITEMW hd = { HDI_FORMAT };
                        Header_GetItem(hHeader, i, &hd);
                        hd.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);          // quita flechas
                        if (i == g_sortColumn)
                            hd.fmt |= (g_sortAsc ? HDF_SORTUP : HDF_SORTDOWN);
                        Header_SetItem(hHeader, i, &hd);
                    }
                    return TRUE;   // ya tratado
                }

                // --- 2) Your existing double-click handler ---
                if (pnm->code == NM_DBLCLK) {
                    HWND hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
                    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                    if (sel != -1) {
                        WCHAR wbuf[MAX_PATH];
                        ListView_GetItemText(hList, sel, 3, wbuf, MAX_PATH);
                        char fileBuf[MAX_PATH];
                        WideCharToMultiByte(
                            CP_UTF8, 0,
                            wbuf, -1,
                            fileBuf, sizeof(fileBuf),
                            nullptr, nullptr
                        );
                        std::string url = "https://modland.com/pub/modules/" + std::string(fileBuf);
                        std::string ddeCmd;
                        if (GetKeyState(VK_MENU) & 0x8000) {
                            ddeCmd = "[open(" + url + ")]";
                        }
                        else {
                            ddeCmd = "[list(" + url + ")]";
                        }
                        xmpfmisc->DDE(ddeCmd.c_str());
                    }
                    return TRUE;
                }

                // --- RCLICK para popup ---
                if (pnm->code == NM_RCLICK) {
                    auto plv = (NMLISTVIEW*)pnm;
                    HWND hList = plv->hdr.hwndFrom;

                    // 1) Hit-test para ver si el click fue sobre un ítem
                    LVHITTESTINFO hti = {};
                    hti.pt = plv->ptAction;  // ya está en coords cliente
                    int idx = ListView_HitTest(hList, &hti);
                    if (idx < 0 || !(hti.flags & LVHT_ONITEM))
                        return FALSE;

                    // 2) selecciona esa fila (y conserva la selección múltiple)
                    ListView_SetItemState(
                        hList,
                        idx,
                        LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED
                    );

                    // 3) cuenta los seleccionados
                    int selCount = ListView_GetSelectedCount(hList);

                    // 4) crea el menú emergente
                    HMENU hPop = CreatePopupMenu();
                    AppendMenuW(hPop, MF_STRING, ID_CONTEXT_OPEN, L"Open");
                    AppendMenuW(hPop, MF_STRING, ID_CONTEXT_ADD, L"Add to playlist");

                    // 5) si hay más de uno, grayea “Open”
                    if (selCount > 1) {
                        EnableMenuItem(
                            hPop,
                            ID_CONTEXT_OPEN,
                            MF_BYCOMMAND | MF_GRAYED
                        );
                    }

                    // 6) dispara el popup
                    POINT pt = plv->ptAction;
                    ClientToScreen(hList, &pt);
                    SetForegroundWindow(hDlg);
                    UINT cmd = TrackPopupMenu(
                        hPop,
                        TPM_RIGHTBUTTON | TPM_RETURNCMD,
                        pt.x, pt.y,
                        0,
                        hDlg,
                        NULL
                    );
                    DestroyMenu(hPop);
                    
                    // HWND hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
                    // Para “Open” solo si hay uno seleccionado
                    if (cmd == ID_CONTEXT_OPEN) {
                        int idx = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                        if (idx != -1) {
                            WCHAR wpath[MAX_PATH];
                            ListView_GetItemText(hList, idx, 3, wpath, MAX_PATH);
                            char pathUtf8[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, pathUtf8, sizeof(pathUtf8), nullptr, nullptr);
                            std::string url = "https://modland.com/pub/modules/" + std::string(pathUtf8);
                            std::string ddeCmd = "[open(" + url + ")]";
                            xmpfmisc->DDE(ddeCmd.c_str());
                        }
                    }
                    // Para “Add to playlist” iteramos todos los seleccionados
                    else if (cmd == ID_CONTEXT_ADD) {
                        int idx = -1;
                        while ((idx = ListView_GetNextItem(hList, idx, LVNI_SELECTED)) != -1) {
                            WCHAR wpath[MAX_PATH];
                            ListView_GetItemText(hList, idx, 3, wpath, MAX_PATH);
                            char pathUtf8[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wpath, -1, pathUtf8, sizeof(pathUtf8), nullptr, nullptr);
                            std::string url = "https://modland.com/pub/modules/" + std::string(pathUtf8);
                            std::string ddeCmd = "[list(" + url + ")]";
                            xmpfmisc->DDE(ddeCmd.c_str());
                        }
                    }
                    return TRUE;
                }
            }
            break;
        }
    }

    return FALSE;
}

static void WINAPI OpenSearchShortcut(void) {
    // std::string cmdOpen = "[open(file:///G:/radix.xm)]";
    // std::string cmdOpen = "[open(G:\\modules\\modarchive\\radix\\radix_[68796]_colours 2.xm)]";
    /*
    std::string cmdOpen = "[open(https://modland.com/pub/modules/Impulsetracker/Arachno/through the night.it)]";
    xmpfmisc->DDE(cmdOpen.c_str());
    cmdOpen = "[list(https://modland.com/pub/modules/Impulsetracker/Chonty/perfect day.it)]";
    xmpfmisc->DDE(cmdOpen.c_str());
    */

    // xmpfmisc->DDE("key2");

    if (!hSearchDlg) {
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icex);

        HWND hMain = (HWND)xmpfmisc->GetWindow();
        hSearchDlg = CreateDialogParam(
            hInstance,
            MAKEINTRESOURCE(IDD_DIALOG1),
            hMain,
            SearchDlgProc,
            0
        );
    }
    if (hSearchDlg) {
        ShowWindow(hSearchDlg, SW_SHOWNORMAL);
        SetForegroundWindow(hSearchDlg);
        HWND hEdit = GetDlgItem(hSearchDlg, IDC_EDIT_SEARCH);
        SetFocus(hEdit);
    }
}
// Entrada del plugin
// extern "C" __declspec(dllexport)
XMPDSP *WINAPI XMPDSP_GetInterface2(DWORD face, InterfaceProc faceproc) {
    if (face != XMPDSP_FACE) return nullptr;
    g_faceproc = faceproc;

    static XMPDSP plugin = {
        XMPDSP_FLAG_NODSP,    // general plugin
        "cmod",               // name
        nullptr,              // About
        Plugin_Init,          // New/init
        Plugin_Exit,          // Free/exit
        Plugin_GetDescription,// GetDescription
        MyConfig,             // Config (diálogo)
        Plugin_GetConfig,     // GetConfig
        Plugin_SetConfig,     // SetConfig
        nullptr,              // NewTrack
        nullptr,              // SetFormat
        nullptr,              // Reset
        nullptr,              // Process
        nullptr               // NewTitle
    };

    // Crear atajo
    xmpfmisc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
    static const XMPSHORTCUT shortcut = {
        0x20001,
        "cmod - Open search dialog",
        OpenSearchShortcut
    };
    xmpfmisc->RegisterShortcut(&shortcut);

    return &plugin;
}

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        hInstance = hDLL;
        DisableThreadLibraryCalls(hDLL);
    }
    return TRUE;
}

