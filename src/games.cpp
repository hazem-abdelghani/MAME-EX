#include "common.h"
#include <unordered_set>
#include <string>
#include <initguid.h>
#include <shobjidl.h>
#include <string>
#include <unordered_map>
#include <vector>

Game *g_games;
int g_ngames;

/* ---------------------------------------------------------------- the games: Sony ZN-1, Sony ZN-2, Namco System 11, Namco System 12, SNK Hyper Neo Geo 64 */
/* generated from the MAME sources by tools/gen_gamedb.py */
static const struct { int id; const wchar_t *set, *parent, *bios, *hw, *title, *maker; int year, flags; } kTable[] = {
#include "gamedb_data.inc"
};

const wchar_t *game_hardware(const Game *g) { return g->hw ? g->hw : L""; }
int game_year(const Game *g) { return g->year; }

int games_load(Game **out, int *n) {
    size_t i, cnt = sizeof kTable / sizeof kTable[0];
    Game *list = (Game *)calloc(cnt, sizeof(Game));
    for (i = 0; i < cnt; i++) {
        Game *g = &list[i];
        g->id = kTable[i].id;
        g->title = wdup(kTable[i].title);
        g->set = wdup(kTable[i].set);
        g->parent = wdup(kTable[i].parent);
        g->bios = wdup(kTable[i].bios);
        g->hw = kTable[i].hw;
        g->maker = kTable[i].maker;
        g->year = kTable[i].year;
        g->flags = kTable[i].flags & GF_NOTWORKING;
        {   /* the Info column: the set this one is a clone of, and what MAME says about it */
            std::wstring info;
            if (kTable[i].parent[0]) info = std::wstring(L"clone of ") + kTable[i].parent;
            if (g->flags & GF_NOTWORKING) { if (!info.empty()) info += L", "; info += L"not working in MAME"; }
            g->info = wdup(info.c_str());
        }
    }
    *out = list;
    *n = (int)cnt;
    return 0;
}

/* What MAME knows is kept in mame-ex-listing-cache.cfg, so the launcher does not have to start MAME at every launch. The cache belongs to one mame.exe
   (path, size, date) and one build of the launcher; another mame.exe, or Rescan Games (F5), asks MAME again. */
typedef std::unordered_map<std::string, std::string> KnownMap;   /* set name -> title as MAME describes it (UTF-8) */

static std::string cache_key(void) {
    wchar_t exe[560];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    char tmp[200];
    char *u;
    std::string k;
    if (!mame_exe(exe, 560) || !GetFileAttributesExW(exe, GetFileExInfoStandard, &fa)) return "";
    u = w_to_u8(exe);
    snprintf(tmp, sizeof tmp, "|%lu|%lu|%lu|%d|" APP_VERSION, (unsigned long)fa.nFileSizeLow, (unsigned long)fa.ftLastWriteTime.dwHighDateTime, (unsigned long)fa.ftLastWriteTime.dwLowDateTime, (int)(sizeof kTable / sizeof kTable[0]));
    k = std::string(u) + tmp;
    free(u);
    return k;
}

static void cache_path(wchar_t *out, size_t cap) { path_join(out, cap, g_root, L"mame-ex-listing-cache.cfg"); }

static int cache_load(const std::string &key, KnownMap &known) {
    wchar_t p[560];
    char *txt, *line, *save = NULL;
    int first = 1, ok = 0;
    cache_path(p, 560);
    txt = read_file(p, NULL);
    if (!txt) return 0;
    for (line = strtok_s(txt, "\n", &save); line; line = strtok_s(NULL, "\n", &save)) {
        char *tab, *cr = strchr(line, '\r');
        if (cr) *cr = 0;
        if (first) { first = 0; if (strncmp(line, "key=", 4) || key != line + 4) break; ok = 1; continue; }
        tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        known[line] = tab + 1;
    }
    free(txt);
    return ok;
}

static void cache_save(const std::string &key, const KnownMap &known) {
    wchar_t p[560];
    Buf b = {0};
    buf_fmt(&b, "key=%s\r\n", key.c_str());
    for (const auto &kv : known) { buf_add(&b, kv.first.c_str()); buf_add(&b, "\t"); buf_add(&b, kv.second.c_str()); buf_add(&b, "\r\n"); }
    cache_path(p, 560);
    write_file(p, b.s, b.n);
    buf_free(&b);
}

/* "mame -listfull" prints every system of MAME with its description: the sets of our table with their descriptions */
static int ask_mame(const Game *g, int n, KnownMap &known) {
    Buf out = {0};
    DWORD code = 0;
    std::unordered_map<std::string, int> idx;
    char *line, *save = NULL;
    int i, lines = 0;
    if (mame_run_capture(L"-listfull", &out, 60000, &code) != 0 || !out.s) { buf_free(&out); return 0; }
    for (i = 0; i < n; i++) { char *u = w_to_u8(g[i].set); idx[u] = i; free(u); }
    for (line = strtok_s(out.s, "\n", &save); line; line = strtok_s(NULL, "\n", &save)) {
        char *sp, *q1, *q2;
        lines++;
        sp = strpbrk(line, " \t");
        if (!sp) continue;
        *sp = 0;
        if (idx.find(line) == idx.end()) continue;
        q1 = strchr(sp + 1, '"');
        q2 = strrchr(sp + 1, '"');
        known[line] = (q1 && q2 && q2 > q1 + 1) ? std::string(q1 + 1, (size_t)(q2 - q1 - 1)) : std::string();
    }
    buf_free(&out);
    return lines > 100;   /* a real listing */
}

/* the sets MAME does not know are marked and cannot be started; the descriptions of the installed version replace the built-in titles */
static void apply_known(Game *g, int n, const KnownMap &known) {
    int i;
    for (i = 0; i < n; i++) {
        char *u = w_to_u8(g[i].set);
        auto it = known.find(u);
        free(u);
        if (it != known.end()) {
            if (!it->second.empty()) {
                wchar_t *t = u8_to_w(it->second.c_str(), -1);
                if (t && t[0]) { free(g[i].title); g[i].title = t; } else free(t);
            }
        } else {
            std::wstring info = g[i].info;
            g[i].flags |= GF_UNKNOWN;
            if (!info.empty()) info += L", ";
            info += L"not in this MAME";
            free(g[i].info);
            g[i].info = wdup(info.c_str());
        }
    }
}

void games_ask_mame(Game *g, int n, int force) {
    KnownMap known;
    std::string key = cache_key();
    if (key.empty()) return;   /* no MAME: nothing to ask */
    if (force || !cache_load(key, known) || known.empty()) {
        known.clear();
        if (!ask_mame(g, n, known)) return;
        cache_save(key, known);
    }
    apply_known(g, n, known);
}

void games_free(Game *g, int n) {
    int i;
    for (i = 0; i < n; i++) { free(g[i].title); free(g[i].info); free(g[i].set); free(g[i].parent); free(g[i].bios); }
    free(g);
}

static int cmp_game(const void *a, const void *b) {
    const Game *x = (const Game *)a, *y = (const Game *)b;
    int c = _wcsicmp(x->title, y->title);
    if (!c) c = _wcsicmp(x->info, y->info);
    if (!c) c = x->id - y->id;
    return c;
}
void games_sort_default(Game *g, int n) { qsort(g, n, sizeof(Game), cmp_game); }

/* ---------------------------------------------------------------- which ROM sets exist */
static std::unordered_set<std::wstring> g_have;   /* the lower case names of the sets (zip / 7z / folder) found
*/
static std::wstring g_biosScan;   /* the BIOS folder (when there is one) */
void rom_available_bios(const wchar_t *dir) { wchar_t t[560]; wcsncpy(t, dir ? dir : L"", 559); t[559] = 0; trim_w(t); g_biosScan = t; }
static std::wstring g_haveSig;   /* the folders and their last write times when they were read: a change shows when a file is added or removed */
static void scan_dir(const wchar_t *dir) {
    wchar_t full[600], pat[640];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    if (is_abs_path(dir)) wcsncpy(full, dir, 599); else path_join(full, 600, g_root, dir);
    full[599] = 0;
    _snwprintf(pat, 640, L"%ls\\*", full);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *n = wdup(fd.cFileName);
        size_t l = wcslen(n);
        if (fd.cFileName[0] == L'.' && (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) { free(n); continue; }
        _wcslwr(n);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {   /* MAME reads zip and 7z sets; an unpacked folder works as well */
            if (l > 4 && !wcscmp(n + l - 4, L".zip")) n[l - 4] = 0;
            else if (l > 3 && !wcscmp(n + l - 3, L".7z")) n[l - 3] = 0;
            else { free(n); continue; }
        }
        g_have.insert(n);
        free(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static std::wstring dirs_signature(const wchar_t *dir) {
    wchar_t *copy, *tok, *save = NULL, def[560], full[600];
    std::wstring sig;
    if (!dir[0]) { roms_dir_abs(def, 560); dir = def; }
    copy = wdup(dir);
    for (tok = wcstok_s(copy, L";", &save); tok; tok = wcstok_s(NULL, L";", &save)) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        trim_w(tok);
        if (!tok[0]) continue;
        if (is_abs_path(tok)) wcsncpy(full, tok, 599); else path_join(full, 600, g_root, tok);
        full[599] = 0;
        sig += full;
        if (GetFileAttributesExW(full, GetFileExInfoStandard, &d)) { wchar_t t[40]; swprintf(t, 40, L"|%lx.%lx", (unsigned long)d.ftLastWriteTime.dwHighDateTime, (unsigned long)d.ftLastWriteTime.dwLowDateTime); sig += t; }
        sig += L";";
    }
    free(copy);
    if (!g_biosScan.empty()) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        if (is_abs_path(g_biosScan.c_str())) wcsncpy(full, g_biosScan.c_str(), 599); else path_join(full, 600, g_root, g_biosScan.c_str());
        full[599] = 0;
        sig += full;
        if (GetFileAttributesExW(full, GetFileExInfoStandard, &d)) { wchar_t t[40]; swprintf(t, 40, L"|%lx.%lx", (unsigned long)d.ftLastWriteTime.dwHighDateTime, (unsigned long)d.ftLastWriteTime.dwLowDateTime); sig += t; }
        sig += L";";
    }
    return sig;
}
/* dir: one ROMs folder or several separated by ; ("" = the roms folder of MAME) */
int rom_available_init(const wchar_t *dir) {
    wchar_t *copy, *tok, *save = NULL, def[560];
    g_have.clear();
    g_haveSig = dirs_signature(dir);
    if (!dir[0]) { roms_dir_abs(def, 560); dir = def; }
    copy = wdup(dir);
    for (tok = wcstok_s(copy, L";", &save); tok; tok = wcstok_s(NULL, L";", &save)) { trim_w(tok); if (tok[0]) scan_dir(tok); }
    free(copy);
    if (!g_biosScan.empty()) scan_dir(g_biosScan.c_str());
    return (int)g_have.size();
}
int rom_available_stale(const wchar_t *dir) { return dirs_signature(dir) != g_haveSig; }
static int have_name(const wchar_t *n) {
    wchar_t *l = wdup(n);
    int r;
    _wcslwr(l);
    r = g_have.count(l) != 0;
    free(l);
    return r;
}
int rom_available(const Game *g) {
    const wchar_t *names[3];
    int i;
    if (!g->set[0]) return 1;
    names[0] = g->set; names[1] = g->parent; names[2] = g->bios;
    for (i = 0; i < 3; i++) if (names[i][0] && !have_name(names[i])) return 0;
    return 1;
}
int rom_missing(const Game *g, wchar_t *out, size_t cap) {
    const wchar_t *names[3];
    int i, n = 0;
    out[0] = 0;
    if (!g->set[0]) return 0;
    names[0] = g->set; names[1] = g->parent; names[2] = g->bios;
    for (i = 0; i < 3; i++) {
        if (!names[i][0] || have_name(names[i])) continue;
        if (n++) wcsncat(out, L", ", cap - wcslen(out) - 1);
        wcsncat(out, names[i], cap - wcslen(out) - 1);
        wcsncat(out, L".zip", cap - wcslen(out) - 1);
    }
    return n;
}
void rom_available_done(void) {
    g_have.clear();
    g_haveSig.clear();
}

/* ---------------------------------------------------------------- shortcut names */
/* "Street Fighter EX Plus (US 970407)" -> "Street Fighter EX Plus (US)": dates, revisions and "Ver. x" are dropped;
   keepVer keeps a version in the name part ("G-Darius Ver.2 (JP)") */
static void drop_trailing_ver(wchar_t *s) {
    size_t i, n = wcslen(s);
    for (i = 0; i < n; i++) {
        size_t k, m;
        if (!iswspace(s[i])) continue;
        k = i;
        while (iswspace(s[k])) k++;
        if (_wcsnicmp(s + k, L"Ver", 3)) { i = k - 1; continue; }
        m = k + 3;
        if (s[m] == L'.') m++;
        while (iswspace(s[m])) m++;
        if (s[m]) {
            size_t e = m;
            while (s[e] && !iswspace(s[e])) e++;
            if (!s[e]) { s[i] = 0; return; }
        }
        i = k - 1;
    }
}
static void squeeze_spaces(wchar_t *s) {
    wchar_t *r = s, *w = s;
    int sp = 0;
    for (; *r; r++) {
        if (iswspace(*r)) { if (!sp) *w++ = L' '; sp = 1; } else { *w++ = *r; sp = 0; }
    }
    *w = 0;
}

void short_title(const wchar_t *title, int keepVer, wchar_t *out, size_t cap) {
    const wchar_t *p;
    wchar_t name[400], region[80];
    for (p = title; (p = wcschr(p, L'(')) != NULL; p++) {
        const wchar_t *q = p + 1, *e;
        size_t rl = 0;
        while (iswspace(*q)) q++;
        while (*q && !iswspace(*q) && *q != L')') { if (rl < 78) region[rl++] = *q; q++; }
        if (rl == 0) continue;
        e = wcschr(q, L')');
        if (!e) continue;
        region[rl] = 0;
        wcsncpy(name, title, p - title);
        name[p - title] = 0;
        trim_w(name);
        if (!keepVer) drop_trailing_ver(name);
        _snwprintf(out, cap, L"%ls (%ls)", name, region);
        out[cap - 1] = 0;
        squeeze_spaces(out);
        return;
    }
    wcsncpy(name, title, 399);
    name[399] = 0;
    trim_w(name);
    if (!keepVer) { drop_trailing_ver(name); trim_w(name); }
    wcsncpy(out, name, cap - 1);
    out[cap - 1] = 0;
}

/* shortTitle, with the version kept only when two games would otherwise get the same name */
void shortcut_name(const Game *g, wchar_t *out, size_t cap) {
    wchar_t n[400], o[400], v[400];
    int i;
    short_title(g->title, 0, n, 400);
    for (i = 0; i < g_ngames; i++) {
        if (g_games[i].id == g->id) continue;
        short_title(g_games[i].title, 0, o, 400);
        if (!_wcsicmp(o, n)) {
            short_title(g->title, 1, v, 400);
            if (_wcsicmp(v, n)) { wcsncpy(out, v, cap - 1); out[cap - 1] = 0; return; }
            wcsncpy(out, g->title, cap - 1);
            out[cap - 1] = 0;
            trim_w(out);
            return;
        }
    }
    wcsncpy(out, n, cap - 1);
    out[cap - 1] = 0;
}

/* ---------------------------------------------------------------- desktop icon */
/* A shortcut on the desktop that starts mame.exe directly with the current settings (fullscreen) and the game's own settings:
   keyboard and controllers work without the launcher. */
int create_desktop_icon(const Game *g, wchar_t *err, size_t errCap) {
    wchar_t name[400], desk[MAX_PATH], lnk[800], self[560], exe[560], dir[560], *joined;
    ArgList a;
    int i;
    HRESULT hr;
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    PlayOpts po;
    err[0] = 0;
    if (!mame_exe(exe, 560)) { _snwprintf(err, errCap, L"mame.exe was not found (General tab > MAME Program)"); return 0; }
    mame_dir(dir, 560);
    shortcut_name(g, name, 400);
    for (i = 0; name[i]; i++) if (wcschr(L"\\/:*?\"<>|", name[i])) { memmove(name + i, name + i + 1, (wcslen(name + i)) * sizeof(wchar_t)); i--; }
    memset(&po, 0, sizeof po);
    po.fullscreen = 1;
    po.noFilter = 1;   /* a shortcut starts MAME alone: no filter window */
    mame_args(g->id, &po, &a);
    joined = args_join(&a);
    args_free(&a);
    GetModuleFileNameW(NULL, self, 560);
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desk))) { _snwprintf(err, errCap, L"Desktop folder not found"); free(joined); return 0; }
    _snwprintf(lnk, 800, L"%ls\\%ls.lnk", desk, name);
    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&sl);
    if (SUCCEEDED(hr)) {
        sl->SetPath(exe);
        sl->SetArguments(joined);
        sl->SetWorkingDirectory(dir);
        sl->SetIconLocation(self, 0);
        sl->SetDescription(g->title);
        hr = sl->QueryInterface(IID_IPersistFile, (void **)&pf);
        if (SUCCEEDED(hr)) { hr = pf->Save(lnk, TRUE); pf->Release(); }
        sl->Release();
    }
    CoUninitialize();
    free(joined);
    if (FAILED(hr)) { _snwprintf(err, errCap, L"Windows could not create the shortcut (error 0x%08lX).", (unsigned long)hr); return 0; }
    return 1;
}
