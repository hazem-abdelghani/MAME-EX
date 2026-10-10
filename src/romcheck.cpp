/* Game > Check ROM Set: looks at the zip files of a game (the game, its parent set and the BIOS) and says what is missing or wrong:
   missing files, damaged zips, wrong checksums. The check runs in a thread so the window stays usable on big sets. */
#include "common.h"
#include "zipio.h"
#include <string>
#include <vector>

#define WM_RC_DONE (WM_APP + 10)

typedef struct {
    HWND dlg;
    wchar_t title[300];
    wchar_t dir[560];   /* the first ROMs folder (the one that is shown) */
    std::vector<std::wstring> dirs;   /* all the ROMs folders: a set is looked for in each of them */
    wchar_t names[3][64];
    const wchar_t *role[3];
    int n;
    wchar_t set[64];
    wchar_t rompath[1200];   /* the -rompath value of the start ("" = what MAME's own settings say) */
} Job;

static void add(Buf *b, const wchar_t *w) { char *u = w_to_u8(w); buf_add(b, u); free(u); }
static void addf(Buf *b, const wchar_t *fmt, ...) {
    wchar_t tmp[1200];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(tmp, 1200, fmt, ap);
    va_end(ap);
    tmp[1199] = 0;
    add(b, tmp);
}

static void size_text(unsigned long v, wchar_t *out, int cap) {
    if (v >= 1024UL * 1024) swprintf(out, cap, L"%.1f MB", v / 1048576.0);
    else swprintf(out, cap, L"%lu KB", (v + 1023) / 1024);
}

/* the first folder that holds the set (name.zip, a folder, name.7z): returns the path, "" when it is in none */
static std::wstring find_set(const Job *j, const wchar_t *name, const wchar_t *ext) {
    size_t d;
    for (d = 0; d < j->dirs.size(); d++) {
        std::wstring p = j->dirs[d] + L"\\" + name + ext;
        if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
    }
    return L"";
}

static DWORD WINAPI check_thread(LPVOID p) {
    Job *j = (Job *)p;
    Buf b = {0};
    int i, bad = 0, missing = 0;
    wchar_t *text;
    addf(&b, L"%ls\r\nROMs folder: %ls\r\n\r\n", j->title, j->dir);
    for (i = 0; i < j->n; i++) {
        wchar_t path[700], sz[40];
        size_t len = 0;
        unsigned char *data;
        ZipReport rep;
        DWORD attr;
        int k;
        {
            std::wstring p = find_set(j, j->names[i], L".zip");
            if (p.empty()) {   /* an unpacked folder? */
                p = find_set(j, j->names[i], L"");
                if (!p.empty() && (GetFileAttributesW(p.c_str()) & FILE_ATTRIBUTE_DIRECTORY)) { addf(&b, L"%ls (%ls): a folder, its files are not checked\r\n", j->names[i], j->role[i]); continue; }
                if (!find_set(j, j->names[i], L".7z").empty()) { addf(&b, L"%ls.7z (%ls): found, a 7z archive: its files are not checked here\r\n", j->names[i], j->role[i]); continue; }
                addf(&b, L"%ls.zip (%ls): MISSING - put %ls.zip into the ROMs folder\r\n", j->names[i], j->role[i], j->names[i]);
                missing++;
                continue;
            }
            wcsncpy(path, p.c_str(), 699); path[699] = 0;
            attr = 0;
        }
        data = (unsigned char *)read_file(path, &len);
        if (!data) { addf(&b, L"%ls.zip (%ls): found, but it cannot be read (in use, or no access?)\r\n", j->names[i], j->role[i]); bad++; continue; }
        if (zip_verify(data, len, &rep)) {
            size_text(rep.unpacked, sz, 40);
            addf(&b, L"%ls.zip (%ls): OK - %d files, %ls\r\n", j->names[i], j->role[i], rep.entries, sz);
        } else {
            addf(&b, L"%ls.zip (%ls): PROBLEM\r\n", j->names[i], j->role[i]);
            for (k = 0; k < rep.nproblems && k < 12; k++) { wchar_t *w = u8_to_w(rep.problems[k], -1); addf(&b, L"    - %ls\r\n", w); free(w); }
            if (rep.nproblems > 12) addf(&b, L"    - ... and %d more\r\n", rep.nproblems - 12);
            bad++;
        }
        free(data);
    }
    if (!missing && !bad) buf_add(&b, "\r\nAll the zip files are there and intact.\r\n");
    else {
        buf_add(&b, "\r\n");
        if (missing) addf(&b, L"%d file(s) missing. ", missing);
        if (bad) addf(&b, L"%d file(s) damaged: download them again.", bad);
        buf_add(&b, "\r\n");
    }
    {   /* what MAME itself says: it also looks inside the sets (file names, checksums) and at the device ROMs */
        wchar_t exe[560], cmd[1600];
        Buf mo = {0};
        DWORD code = 0;
        buf_add(&b, "\r\nMAME's own check (-verifyroms):\r\n");
        if (!mame_exe(exe, 560)) buf_add(&b, "mame.exe was not found (General tab > MAME Program), so this check could not run.\r\n");
        else {
            size_t rl = wcslen(j->rompath);
            if (rl && j->rompath[rl - 1] == L'\\' && rl < 1198) wcscat(j->rompath, L"\\");   /* a trailing backslash (D:\) must not swallow the closing quote */
            if (j->rompath[0]) _snwprintf(cmd, 1600, L"-verifyroms %ls -rompath \"%ls\"", j->set, j->rompath);
            else _snwprintf(cmd, 1600, L"-verifyroms %ls", j->set);
            cmd[1599] = 0;
            if (mame_run_capture(cmd, &mo, 60000, &code) != 0) buf_add(&b, "MAME did not answer.\r\n");
            else {
                char *p, *q;
                if (mo.s) {   /* the lines end with a plain \n: the edit box wants \r\n */
                    for (p = mo.s; *p; p = q) {
                        q = strchr(p, '\n');
                        if (!q) { buf_add(&b, p); break; }
                        buf_addn(&b, p, (size_t)(q - p));
                        if (q > p && q[-1] == '\r') ; else buf_add(&b, "\r");
                        buf_add(&b, "\n");
                        q++;
                    }
                }
                if (code == 0) buf_add(&b, "\r\nMAME is happy with this set: it should start.");
                else addf(&b, L"\r\nMAME found a problem (exit code %lu): see the lines above.", (unsigned long)code);
            }
        }
        buf_free(&mo);
    }
    text = u8_to_w(b.s ? b.s : "", -1);
    buf_free(&b);
    if (!PostMessageW(j->dlg, WM_RC_DONE, 0, (LPARAM)text)) free(text);
    delete j;
    return 0;
}

static LRESULT CALLBACK dummy_unused(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

void dlg_check_roms(HWND owner, const Game *g) {
    Modal md;
    Job *j = new Job();
    const wchar_t *cand[3], *role[3] = {L"game", L"parent set", L"BIOS"};
    int i, k;
    (void)dummy_unused;
    memset(&md, 0, sizeof md);
    md.kind = 7; md.owner = owner;
    wcscpy(md.title, L"Check ROM Set");
    make_modal(&md, 640, 460);
    md.c1 = mkat(md.dlg, L"EDIT", L"Checking the files…", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, 10, 10, 620, 400, 0);
    SetWindowLongW(md.c1, GWL_EXSTYLE, GetWindowLongW(md.c1, GWL_EXSTYLE) | WS_EX_CLIENTEDGE);
    SetWindowPos(md.c1, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    mkat(md.dlg, L"BUTTON", L"Close", BS_DEFPUSHBUTTON | WS_TABSTOP, 550, 420, 80, 26, IDCANCEL);
    cand[0] = g->set; cand[1] = g->parent; cand[2] = g->bios;
    j->dlg = md.dlg;
    wcsncpy(j->title, g->title, 299);
    wcsncpy(j->set, g->set, 63);
    {   /* the -rompath of a start, so MAME looks where the launcher looks */
        ArgList a;
        int ai;
        mame_args(g->id, NULL, &a);
        for (ai = 0; ai + 1 < a.n; ai++) if (!wcscmp(a.a[ai], L"-rompath")) { wcsncpy(j->rompath, a.a[ai + 1], 1199); break; }
        args_free(&a);
    }
    roms_dir_abs(j->dir, 560);
    {   /* every folder of the ROMs setting (MAME and the launcher look in all of them) */
        wchar_t cp[1200], *tok, *save = NULL, ab[700];
        wcsncpy(cp, g_set.roms, 1199); cp[1199] = 0;
        for (tok = wcstok_s(cp, L";", &save); tok; tok = wcstok_s(NULL, L";", &save)) { trim_w(tok); if (tok[0]) { if (is_abs_path(tok)) wcsncpy(ab, tok, 699); else path_join(ab, 700, g_root, tok); ab[699] = 0; j->dirs.push_back(ab); } }
        if (j->dirs.empty()) j->dirs.push_back(j->dir);
        if (g_set.biosDir[0]) { if (is_abs_path(g_set.biosDir)) wcsncpy(ab, g_set.biosDir, 699); else path_join(ab, 700, g_root, g_set.biosDir); ab[699] = 0; j->dirs.push_back(ab); }   /* the BIOS folder: BIOS sets are looked for there too */
    }
    for (i = 0; i < 3; i++) {
        int dup = 0;
        if (!cand[i][0]) continue;
        for (k = 0; k < j->n; k++) if (!_wcsicmp(j->names[k], cand[i])) dup = 1;
        if (dup) continue;
        wcsncpy(j->names[j->n], cand[i], 63);
        j->role[j->n++] = role[i];
    }
    if (j->n == 0) {
        SetWindowTextW(md.c1, L"This game has no ROM set name.");
        delete j;
    } else CloseHandle(CreateThread(NULL, 0, check_thread, j, 0, NULL));
    run_modal(&md);
}
