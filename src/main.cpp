/* MAME EX: the launcher window. Games of Sony ZN-1 / ZN-2 and the boards built on them, Namco System 10 / 11 / 12, SNK Hyper Neo Geo 64, Konami GV / System 573 and Taito G-Net, started with MAME. */
#include "common.h"
#include <uxtheme.h>
#include <shlobj.h>
#include <commdlg.h>
#include "classify.h"
#include <algorithm>


enum { PG_SYSTEM, PG_VIDEO, PG_AUDIO, PG_CONTROLS, PG_COMBOS, NPAGES };   /* the tabs of the right pane, in order */

typedef struct { HWND lbl, ctl, btn, clr; int kind; } PRow;   /* kind 0: label + combo, 1: label + edit (btn: Browse, clr: X, to the right of it), 2: wide check box, 3: wide button, 5: group title, 6: wide note text */

/* ---------------------------------------------------------------- state */
static struct {
    HWND status, tip, tab, page[NPAGES];
    HWND cbRegion, cbMaker, cbGenre, cbHw;
    HWND lblSearch, search, clearBtn, sortBtn, lv, favOnly, iconBtn, stopBtn, playBtn, saveBtn, cancelBtn, defBtn;
    HWND tbRefresh, tbFav, tbAvail, tbFs, tbSet, tbVideo, tbAudio, tbCtl, tbAdv, tbSep1, tbSep2;
    /* general */
    HWND eMame, bMameBrowse, bMameClear, eRoms, bRomBrowse, bRomClear, eBios, bBiosBrowse, bBiosClear, chAvail, chHide, chLogs, cbOnStart, chClassic, cbTheme;
    /* video */
    HWND cbMode, chFull, cbRes, cbRot, chAspect, chUneven, chFilter, cbPrescale, chVsync, chSyncRef, chThrottle, cbFskip, chMax, cbChain, cbBackend, chCrop, chHlsl, chGlsl, cbGlslF, eGlslS, cbIntScale, chFx, bHlslSet, bMoreOpts, cbFxScaler, chFxFxaa, cbFxDither;
    /* audio */
    HWND cbSound, cbRate, eVol, cbLat;
    /* controls */
    HWND chJoy, cbProv, chMouse, chGun, chContr, chOvGame, cbAf, eDead, eSat, cbCtrlr, cbProf, bPSave, bPSaveAs, bPRen, bPDel, bOpenCfg, lvBind, bSetKey, bSetPad, bBindClear, bBindDef;
    /* advanced */
    HWND chSkip, chSkipW, chSave, chCheat, chQuit, eExtra;
    PRow vrows[64], arows[8], srows[40], crows[24], xrows[8];
    int nv, na, ns, nc, nx;
} W;

typedef struct { int w, h; } Mode;
static Mode *g_modes;
static int g_nmodes;

static int g_applying, g_syncing, g_rebuilding;
static char *g_savedSig;
static PROCESS_INFORMATION g_proc;
static void sync_main_enable(void);
static void hide_opt(int activateMain);
static int g_mameWarned, g_lastGameId = -1;
static int g_firstRun;   /* there was no settings file at the start: the key / pad mapping starts with the launcher's defaults */
static int g_romsKnown;   /* the ROM sets were read once */
static int g_running, g_stopped, g_minimized;   /* g_minimized: the launcher was minimized for the game */
static DWORD g_startTick;
static int g_splitX, g_dragging;
static HWND g_opt;            /* the options window (the tabs in a window of their own) */
static HMENU g_optsMenu;      /* the Options menu of the menu bar */
static HMENU g_viewMenu, g_themeMenu, g_colsMenu;
static int g_searchOv = -1;   /* for this session only: 1 the search bar was shown by Ctrl+F, 0 it was closed with Close; -1 as in the View menu (remembered) */
#define SEARCH_SHOWN() (g_searchOv >= 0 ? g_searchOv : !g_set.hideSearch)
static int g_optWin;          /* 1: the tabs are in the options window, 0: in the main window (the classic layout) */
#define g_sortCol (g_set.sortCol)
#define NCOLS 7   /* the columns of the game list: Favorite, #, Game, Info, Status, Hardware, Year */

#define g_sortAsc (g_set.sortAsc)
static int *g_view, g_nview;
static HMENU g_recentMenu;
static wchar_t g_note[200];
static wchar_t g_ctrlrNames[64][64];
static int g_nctrlr;
static int g_columnsSized;
static wchar_t g_regions[8][8], g_makers[32][40], g_genres[16][24], g_hws[32][40];   /* the makers and genres of the listed games, for the filters */
static int g_nreg, g_nmak, g_ngen, g_nhw;

/* ---------------------------------------------------------------- helpers */
static void set_status(const wchar_t *s) { SendMessageW(W.status, SB_SETTEXTW, 0, (LPARAM)s); }
static void get_status(wchar_t *s, int cap) {   /* at most cap - 1 characters, whatever the length of the text */
    int n = LOWORD(SendMessageW(W.status, SB_GETTEXTLENGTHW, 0, 0));
    wchar_t *tmp = (wchar_t *)calloc(n + 2, sizeof(wchar_t));
    SendMessageW(W.status, SB_GETTEXTW, 0, (LPARAM)tmp);
    wcsncpy(s, tmp, cap - 1);
    s[cap - 1] = 0;
    free(tmp);
}
static void set_note(const wchar_t *s) {
    wcsncpy(g_note, s, 199);
    set_status(s);
    SetTimer(g_main, 2, 5000, NULL);
}
static int playable(const Game *g);
static void selection_status(void);
static void fill_filters(void);
/* "71 games supported, 12 with ROMs": how many of the games have their ROM sets in the ROMs folder */
static void games_status(void) {
    wchar_t t[120];
    int i, have = 0;
    for (i = 0; i < g_ngames; i++) if (rom_available(&g_games[i])) have++;
    swprintf(t, 120, L"%d games (PSX-based and Hyper Neo Geo 64 boards), %d with ROMs", g_ngames, have);
    set_status(t);
}

/* enabling / disabling a control inside a page: the page background must be painted again under the label text */
static void enable_ctl(HWND c, BOOL on) {
    RECT rc;
    if (!c) return;
    wchar_t cls[16];
    if (GetClassNameW(c, cls, 16) && !_wcsicmp(cls, L"Static")) SetPropW(c, L"dim", (HANDLE)(INT_PTR)(!on));   /* a disabled label would get Windows' white emboss: it is only drawn grey */
    else EnableWindow(c, on);
    GetWindowRect(c, &rc);
    MapWindowPoints(NULL, GetParent(c), (POINT *)&rc, 2);
    InvalidateRect(GetParent(c), &rc, TRUE);
    InvalidateRect(c, NULL, TRUE);   /* a label is not disabled, only drawn grey: it has to repaint itself */
}

static int selected_game_index(void) {
    int i = ListView_GetNextItem(W.lv, -1, LVNI_SELECTED);
    LVITEMW it;
    if (i < 0) return -1;
    memset(&it, 0, sizeof it);
    it.mask = LVIF_PARAM;
    it.iItem = i;
    ListView_GetItem(W.lv, &it);
    return (int)it.lParam;
}
static const Game *selected_game(void) { int i = selected_game_index(); return i >= 0 && i < g_ngames ? &g_games[i] : NULL; }

/* ---------------------------------------------------------------- display modes */
static int cmp_mode(const void *a, const void *b) { const Mode *x = (const Mode *)a, *y = (const Mode *)b; return x->w != y->w ? x->w - y->w : x->h - y->h; }
static void load_modes(void) {
    DEVMODEW dm;
    DWORD i;
    int cap = 0, k;
    for (i = 0;; i++) {
        memset(&dm, 0, sizeof dm);
        dm.dmSize = sizeof dm;
        if (!EnumDisplaySettingsW(NULL, i, &dm)) break;
        if (dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480 && dm.dmBitsPerPel >= 16) {
            for (k = 0; k < g_nmodes; k++) if (g_modes[k].w == (int)dm.dmPelsWidth && g_modes[k].h == (int)dm.dmPelsHeight) break;
            if (k < g_nmodes) continue;
            if (g_nmodes == cap) { cap = cap ? cap * 2 : 32; g_modes = (Mode *)realloc(g_modes, cap * sizeof(Mode)); }
            g_modes[g_nmodes].w = dm.dmPelsWidth; g_modes[g_nmodes].h = dm.dmPelsHeight; g_nmodes++;
        }
    }
    if (!g_nmodes) {
        static const Mode fb[] = {{640, 480}, {800, 600}, {1024, 768}, {1152, 864}, {1280, 720}, {1280, 960}, {1280, 1024}, {1366, 768}, {1600, 900}, {1600, 1200}, {1920, 1080}};
        g_modes = (Mode *)malloc(sizeof fb);
        memcpy(g_modes, fb, sizeof fb);
        g_nmodes = (int)(sizeof fb / sizeof fb[0]);
    }
    qsort(g_modes, g_nmodes, sizeof(Mode), cmp_mode);
}

int ui_mode_count(void) { return g_nmodes; }
void ui_mode_get(int i, int *w, int *h) { *w = g_modes[i].w; *h = g_modes[i].h; }

/* ---------------------------------------------------------------- game list */
static int cmp_view(const void *a, const void *b) {
    const Game *x = &g_games[*(const int *)a], *y = &g_games[*(const int *)b];
    int c = 0, d = g_sortAsc ? 1 : -1;
    if (g_sortCol == 1) c = x->id - y->id;
    else if (g_sortCol == 4) { c = rom_available(x) - rom_available(y); if (!c) c = _wcsicmp(x->title, y->title); }   /* Status: the missing ones first */
    else if (g_sortCol == 5) { c = _wcsicmp(game_hardware(x), game_hardware(y)); if (!c) c = _wcsicmp(x->title, y->title); }
    else if (g_sortCol == 6) { c = game_year(x) - game_year(y); if (!c) c = _wcsicmp(x->title, y->title); }
    else {
        if (g_sortCol == 0) {   /* favorites first (ascending), then by name */
            int fa = il_has(&g_set.fav, x->id), fb = il_has(&g_set.fav, y->id);
            if (fa != fb) return (fa ? -1 : 1) * d;
        }
        c = _wcsicmp(x->title, y->title);
        if (!c) c = _wcsicmp(x->info, y->info);
    }
    if (!c) c = x->id - y->id;
    return c * d;
}

static void sort_arrow(void) {
    HWND hdr = ListView_GetHeader(W.lv);
    int i;
    for (i = 0; i < NCOLS; i++) {
        HDITEMW h;
        memset(&h, 0, sizeof h);
        h.mask = HDI_FORMAT;
        Header_GetItem(hdr, i, &h);
        h.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == g_sortCol) h.fmt |= g_sortAsc ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(hdr, i, &h);
    }
}

/* ---- game list columns: hidden ones have width 0 (their width is kept in g_colW to show them again) */
static int g_colW[NCOLS];
static const int kColOrder[NCOLS] = {1, 4, 0, 2, 5, 6, 3};   /* the order the columns are shown in: #, Status, Favorite, Game, Hardware, Year, Info */
static const wchar_t *const kColNames[NCOLS] = {L"Favorite", L"#", L"Game", L"Info", L"Status", L"Hardware", L"Year"};   /* by column number */

static void refilter(void);
static void apply_columns(void) {
    int k, n, i;
    g_set.colsHidden &= ~4;   /* the Game column always stays */
    g_lvProg++;
    for (k = 0; k < NCOLS; k++) {
        int w = ListView_GetColumnWidth(W.lv, k);
        if ((g_set.colsHidden >> k) & 1) { if (w > 0) g_colW[k] = w; if (w != 0) ListView_SetColumnWidth(W.lv, k, 0); }
        else if (w == 0) ListView_SetColumnWidth(W.lv, k, g_colW[k] > 0 ? g_colW[k] : S(k == 0 ? 64 : k == 1 ? 40 : k == 2 ? 260 : k == 3 ? 140 : k == 4 ? 64 : k == 5 ? 190 : 64));
    }
    g_lvProg--;
    n = ListView_GetItemCount(W.lv);   /* the Status icons follow the column: not shown when it is hidden */
    for (i = 0; i < n; i++) {
        LVITEMW li;
        LVITEMW si;
        memset(&li, 0, sizeof li);
        li.mask = LVIF_PARAM; li.iItem = i;
        if (!ListView_GetItem(W.lv, &li) || li.lParam < 0 || li.lParam >= g_ngames) continue;
        memset(&si, 0, sizeof si);
        si.mask = LVIF_IMAGE; si.iItem = i; si.iSubItem = 4;
        si.iImage = ((g_set.colsHidden >> 4) & 1) ? I_IMAGENONE : rom_available(&g_games[li.lParam]) ? 0 : 1;
        ListView_SetItem(W.lv, &si);
    }
}

/* right-click on the title of the list: which columns are shown */
static LRESULT CALLBACK colhdr_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    (void)id; (void)ref;
    if (m == WM_RBUTTONUP) {
        HMENU pm = CreatePopupMenu();
        POINT pt = {(short)LOWORD(l), (short)HIWORD(l)};
        int k;
        for (k = 0; k < NCOLS; k++) { int c = kColOrder[k]; AppendMenuW(pm, MF_STRING | (c == 2 ? MF_GRAYED : 0) | (((g_set.colsHidden >> c) & 1) ? 0 : MF_CHECKED), ID_COL0 + c, kColNames[c]); }   /* Game always stays */
        ClientToScreen(h, &pt);
        TrackPopupMenu(pm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, NULL);
        DestroyMenu(pm);
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

/* the title notifies the list: a hidden column cannot be dragged open */
static LRESULT CALLBACK lvhdr_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    (void)id; (void)ref;
    if (m == WM_NOTIFY) {
        NMHEADERW *nh = (NMHEADERW *)l;
        if ((nh->hdr.code == HDN_BEGINTRACKW || nh->hdr.code == HDN_BEGINTRACKA || nh->hdr.code == HDN_DIVIDERDBLCLICKW || nh->hdr.code == HDN_DIVIDERDBLCLICKA) &&
            nh->iItem >= 0 && nh->iItem < NCOLS && ((g_set.colsHidden >> nh->iItem) & 1)) return TRUE;
    }
    return DefSubclassProc(h, m, w, l);
}

static void size_columns(void) {
    HDC dc;
    TEXTMETRICW tm;
    HGDIOBJ old;
    int ch, k;
    static const int chars[] = {6, 4, 53, 34, 8, 27, 5};
    if (g_columnsSized) return;
    dc = GetDC(W.lv);
    old = SelectObject(dc, g_font);
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(W.lv, dc);
    ch = tm.tmAveCharWidth;
    if (ch <= 0) return;
    if (g_set.colsGame[1] > 0) { lv_set_widths(W.lv, g_set.colsGame, NCOLS); g_columnsSized = 1; apply_columns(); return; }   /* the widths of the last session */
    for (k = 0; k < NCOLS; k++) ListView_SetColumnWidth(W.lv, k, std::max(chars[k] * ch + 12 + (k == 0 ? S(24) : 0), lv_min_w(W.lv, k)));   /* a little room for the cell margins (and the check box) */
    g_columnsSized = 1;
    apply_columns();
}

static int contains_ci(const wchar_t *hay, const wchar_t *needleLower) {
    wchar_t *l = wdup(hay);
    int r;
    _wcslwr(l);
    r = wcsstr(l, needleLower) != NULL;
    free(l);
    return r;
}

/* the "All Makers" / "All Genres" lists: what the games of the list really have (guessed from the title and BIOS) */
static int cmp_w(const void *a, const void *b) { return _wcsicmp((const wchar_t *)a, (const wchar_t *)b); }
static void fill_filters(void) {
    wchar_t keepR[8] = L"", keepM[40] = L"", keepG[24] = L"", keepH[40] = L"";
    int i, j, sel;
    int ir = (int)SendMessageW(W.cbRegion, CB_GETCURSEL, 0, 0);
    if (ir > 0 && ir <= g_nreg) wcscpy(keepR, g_regions[ir - 1]);
    int im = (int)SendMessageW(W.cbMaker, CB_GETCURSEL, 0, 0), ig = (int)SendMessageW(W.cbGenre, CB_GETCURSEL, 0, 0);
    if (im > 0 && im <= g_nmak) wcscpy(keepM, g_makers[im - 1]);
    if (ig > 0 && ig <= g_ngen) wcscpy(keepG, g_genres[ig - 1]);
    { int ih = (int)SendMessageW(W.cbHw, CB_GETCURSEL, 0, 0); if (ih > 0 && ih <= g_nhw) wcscpy(keepH, g_hws[ih - 1]); }
    if (!g_nreg && !g_nmak && !g_ngen && !g_nhw) { wcsncpy(keepR, g_set.fRegion, 7); wcsncpy(keepM, g_set.fMaker, 39); wcsncpy(keepG, g_set.fGenre, 23); wcsncpy(keepH, g_set.fHw, 39); }   /* the first fill: the filters of the last session */
    g_nreg = g_nmak = g_ngen = g_nhw = 0;
    for (i = 0; i < g_ngames; i++) {
        const wchar_t *m = classify_maker(g_games[i].maker), *g = classify_genre(g_games[i].title);
        const wchar_t *rg = classify_region(g_games[i].title);
        for (j = 0; j < g_nreg; j++) if (!wcscmp(g_regions[j], rg)) break;
        if (j == g_nreg && g_nreg < 8) wcsncpy(g_regions[g_nreg++], rg, 7);
        for (j = 0; j < g_nmak; j++) if (!wcscmp(g_makers[j], m)) break;
        if (j == g_nmak && g_nmak < 32) wcsncpy(g_makers[g_nmak++], m, 39);
        for (j = 0; j < g_ngen; j++) if (!wcscmp(g_genres[j], g)) break;
        if (j == g_ngen && g_ngen < 16) wcsncpy(g_genres[g_ngen++], g, 23);
        {   const wchar_t *hw = game_hardware(&g_games[i]);
            for (j = 0; j < g_nhw; j++) if (!wcscmp(g_hws[j], hw)) break;
            if (j == g_nhw && g_nhw < 32) wcsncpy(g_hws[g_nhw++], hw, 39);
        }
    }
    qsort(g_regions, (size_t)g_nreg, sizeof g_regions[0], cmp_w);
    qsort(g_makers, (size_t)g_nmak, sizeof g_makers[0], cmp_w);
    qsort(g_genres, (size_t)g_ngen, sizeof g_genres[0], cmp_w);
    qsort(g_hws, (size_t)g_nhw, sizeof g_hws[0], cmp_w);
    SendMessageW(W.cbRegion, CB_RESETCONTENT, 0, 0);
    SendMessageW(W.cbRegion, CB_ADDSTRING, 0, (LPARAM)L"All Regions");
    for (i = 0, sel = 0; i < g_nreg; i++) { SendMessageW(W.cbRegion, CB_ADDSTRING, 0, (LPARAM)g_regions[i]); if (!wcscmp(keepR, g_regions[i])) sel = i + 1; }
    SendMessageW(W.cbRegion, CB_SETCURSEL, sel, 0);
    SendMessageW(W.cbMaker, CB_RESETCONTENT, 0, 0);
    SendMessageW(W.cbMaker, CB_ADDSTRING, 0, (LPARAM)L"All Makers");
    for (i = 0, sel = 0; i < g_nmak; i++) { SendMessageW(W.cbMaker, CB_ADDSTRING, 0, (LPARAM)g_makers[i]); if (!wcscmp(keepM, g_makers[i])) sel = i + 1; }
    SendMessageW(W.cbMaker, CB_SETCURSEL, sel, 0);
    SendMessageW(W.cbGenre, CB_RESETCONTENT, 0, 0);
    SendMessageW(W.cbGenre, CB_ADDSTRING, 0, (LPARAM)L"All Genres");
    for (i = 0, sel = 0; i < g_ngen; i++) { SendMessageW(W.cbGenre, CB_ADDSTRING, 0, (LPARAM)g_genres[i]); if (!wcscmp(keepG, g_genres[i])) sel = i + 1; }
    SendMessageW(W.cbGenre, CB_SETCURSEL, sel, 0);
    SendMessageW(W.cbHw, CB_RESETCONTENT, 0, 0);
    SendMessageW(W.cbHw, CB_ADDSTRING, 0, (LPARAM)L"All Hardware");
    for (i = 0, sel = 0; i < g_nhw; i++) { SendMessageW(W.cbHw, CB_ADDSTRING, 0, (LPARAM)g_hws[i]); if (!wcscmp(keepH, g_hws[i])) sel = i + 1; }
    SendMessageW(W.cbHw, CB_SETCURSEL, sel, 0);
}

static void refilter(void) {
    wchar_t q[200], rom[520], selRegion[8] = L"", selMaker[40] = L"", selGenre[24] = L"", selHw[40] = L"";
    int favOnly = ctl_checked(W.favOnly), avail, i, selId = -1;
    const Game *sg = selected_game();
    if (sg) selId = sg->id;
    {
        int im = (int)SendMessageW(W.cbMaker, CB_GETCURSEL, 0, 0), ig = (int)SendMessageW(W.cbGenre, CB_GETCURSEL, 0, 0);
        int irg = (int)SendMessageW(W.cbRegion, CB_GETCURSEL, 0, 0);
        if (irg > 0 && irg <= g_nreg) wcscpy(selRegion, g_regions[irg - 1]);
        if (im > 0 && im <= g_nmak) wcscpy(selMaker, g_makers[im - 1]);
        if (ig > 0 && ig <= g_ngen) wcscpy(selGenre, g_genres[ig - 1]);
        { int ih = (int)SendMessageW(W.cbHw, CB_GETCURSEL, 0, 0); if (ih > 0 && ih <= g_nhw) wcscpy(selHw, g_hws[ih - 1]); }
    }
    if (g_nreg || g_nmak || g_ngen || g_nhw) { wcscpy(g_set.fRegion, selRegion); wcscpy(g_set.fMaker, selMaker); wcscpy(g_set.fGenre, selGenre); wcscpy(g_set.fHw, selHw); }   /* not before the first fill: the saved filters are still waiting */
    GetWindowTextW(W.search, q, 200);
    trim_w(q);
    _wcslwr(q);
    GetWindowTextW(W.eRoms, rom, 520);
    { wchar_t bd[520]; GetWindowTextW(W.eBios, bd, 520); rom_available_bios(bd); }
    avail = ctl_checked(W.chAvail);
    tb_set_checked(W.tbAvail, avail);
    if (!g_romsKnown || rom_available_stale(rom)) { rom_available_init(rom); g_romsKnown = 1; }   /* kept until the next refresh: the Play button needs it too (an empty box: the roms folder of MAME) */
    free(g_view);
    g_view = (int *)malloc((g_ngames + 1) * sizeof(int));
    g_nview = 0;
    for (i = 0; i < g_ngames; i++) {
        const Game *g = &g_games[i];
        if (q[0]) {
            wchar_t idt[16];
            swprintf(idt, 16, L"%d", g->id);
            if (!contains_ci(g->title, q) && !contains_ci(g->info, q) && wcscmp(idt, q)) continue;
        }
        if (favOnly && !il_has(&g_set.fav, g->id)) continue;
        if (avail && !rom_available(g)) continue;
        if (selRegion[0] && wcscmp(selRegion, classify_region(g->title))) continue;
        if (selMaker[0] && wcscmp(selMaker, classify_maker(g->maker))) continue;
        if (selGenre[0] && wcscmp(selGenre, classify_genre(g->title))) continue;
        if (selHw[0] && wcscmp(selHw, game_hardware(g))) continue;
        g_view[g_nview++] = i;
    }
    qsort(g_view, g_nview, sizeof(int), cmp_view);

    g_rebuilding = 1;
    SendMessageW(W.lv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(W.lv);
    for (i = 0; i < g_nview; i++) {
        const Game *g = &g_games[g_view[i]];
        wchar_t id[16];
        LVITEMW it;
        swprintf(id, 16, L"%d", g->id);
        memset(&it, 0, sizeof it);
        it.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
        it.iImage = I_IMAGENONE;
        it.iItem = i;
        it.pszText = (LPWSTR)L"";
        it.lParam = g_view[i];
        ListView_InsertItem(W.lv, &it);
        ListView_SetItemText(W.lv, i, 1, id);
        ListView_SetItemText(W.lv, i, 2, g->title);
        ListView_SetItemText(W.lv, i, 3, g->info);
        {   /* Status: an icon, a tick when the ROM set is there, a cross when it is not (none when the column is hidden: an icon is not cut off with the width) */
            LVITEMW si;
            memset(&si, 0, sizeof si);
            si.mask = LVIF_IMAGE; si.iItem = i; si.iSubItem = 4; si.iImage = ((g_set.colsHidden >> 4) & 1) ? I_IMAGENONE : rom_available(g) ? 0 : 1;
            ListView_SetItem(W.lv, &si);
        }
        ListView_SetItemText(W.lv, i, 5, (wchar_t *)game_hardware(g));
        if (game_year(g)) { wchar_t yr[8]; swprintf(yr, 8, L"%d", game_year(g)); ListView_SetItemText(W.lv, i, 6, yr); }
        ListView_SetCheckState(W.lv, i, il_has(&g_set.fav, g->id));
        if (g->id == selId) ListView_SetItemState(W.lv, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    SendMessageW(W.lv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(W.lv, NULL, TRUE);
    g_rebuilding = 0;
    size_columns();
    sort_arrow();
}

/* ---------------------------------------------------------------- settings <-> widgets */
static void set_combo(HWND cb, int v) {
    int n = (int)SendMessageW(cb, CB_GETCOUNT, 0, 0);
    if (v >= 0 && v < n) SendMessageW(cb, CB_SETCURSEL, v, 0);
}
static int get_combo(HWND cb) { int i = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0); return i < 0 ? 0 : i; }

/* the controller files of MAME's ctrlr folder: "None" first */
static void ctrlr_fill(const wchar_t *keep) {
    int i, sel = 0;
    g_nctrlr = mame_ctrlr_names(g_ctrlrNames, 64);
    SendMessageW(W.cbCtrlr, CB_RESETCONTENT, 0, 0);
    SendMessageW(W.cbCtrlr, CB_ADDSTRING, 0, (LPARAM)L"None");
    for (i = 0; i < g_nctrlr; i++) { SendMessageW(W.cbCtrlr, CB_ADDSTRING, 0, (LPARAM)g_ctrlrNames[i]); if (!_wcsicmp(keep, g_ctrlrNames[i])) sel = i + 1; }
    if (keep[0] && !sel) {   /* a controller file that is not in the folder (any more) stays in the list */
        SendMessageW(W.cbCtrlr, CB_ADDSTRING, 0, (LPARAM)keep);
        sel = g_nctrlr + 1;
    }
    SendMessageW(W.cbCtrlr, CB_SETCURSEL, sel, 0);
}

static void update_state(void) {   /* options that do not apply to the current choices are greyed out */
    int fs = ctl_checked(W.chFull);
    enable_ctl(W.chMax, !fs);
    enable_ctl(W.chHlsl, get_combo(W.cbMode) == 2);   /* HLSL works with the Direct3D video mode only */
    enable_ctl(W.bHlslSet, get_combo(W.cbMode) == 2 && ctl_checked(W.chHlsl));
    {   /* the filter window: its options need it ticked */
        int on = ctl_checked(W.chFx);
        enable_ctl(W.cbFxScaler, on); enable_ctl(W.chFxFxaa, on); enable_ctl(W.cbFxDither, on);
        enable_ctl((HWND)GetPropW(W.cbFxScaler, L"lbl"), on); enable_ctl((HWND)GetPropW(W.cbFxDither, L"lbl"), on);
    }
    {   /* GLSL: the OpenGL video mode only; its filter and shader need it ticked */
        int gl = get_combo(W.cbMode) == 3, on = gl && ctl_checked(W.chGlsl);
        enable_ctl(W.chGlsl, gl);
        enable_ctl(W.cbGlslF, on); enable_ctl(W.eGlslS, on);
        enable_ctl((HWND)GetPropW(W.cbGlslF, L"lbl"), on); enable_ctl((HWND)GetPropW(W.eGlslS, L"lbl"), on);
    }
    {   /* the BGFX options need the BGFX video mode (or Auto, which usually picks it) */
        int m = get_combo(W.cbMode), on = m == 0 || m == 1;
        enable_ctl(W.cbChain, on); enable_ctl(W.cbBackend, on);
        enable_ctl(W.cbChain ? (HWND)GetPropW(W.cbChain, L"lbl") : NULL, on); enable_ctl(W.cbBackend ? (HWND)GetPropW(W.cbBackend, L"lbl") : NULL, on);
    }
    {
        int j = ctl_checked(W.chJoy);
        enable_ctl(W.cbProv, j); enable_ctl(W.chContr, j); enable_ctl(W.eDead, j); enable_ctl(W.eSat, j);
        enable_ctl((HWND)GetPropW(W.cbProv, L"lbl"), j); enable_ctl((HWND)GetPropW(W.eDead, L"lbl"), j); enable_ctl((HWND)GetPropW(W.eSat, L"lbl"), j);
    }
    {
        int snd = get_combo(W.cbSound) != 1;
        enable_ctl(W.cbRate, snd); enable_ctl(W.eVol, snd); enable_ctl(W.cbLat, snd);
        enable_ctl((HWND)GetPropW(W.cbRate, L"lbl"), snd); enable_ctl((HWND)GetPropW(W.eVol, L"lbl"), snd); enable_ctl((HWND)GetPropW(W.cbLat, L"lbl"), snd);
    }
}

static void update_buttons(void);
static char *ui_sig(void);

/* the ROM sets of the games are looked up again (the ROMs folder may have changed) */
static int roms_refresh(int force) {   /* 1 when the sets were read again (the status icons then need a refresh) */
    wchar_t rom[520];
    GetWindowTextW(W.eRoms, rom, 520);
    { wchar_t bd[520]; GetWindowTextW(W.eBios, bd, 520); rom_available_bios(bd); }
    if (!force && g_romsKnown && !rom_available_stale(rom)) return 0;
    rom_available_init(rom);
    g_romsKnown = 1;
    return 1;
}
static int playable(const Game *g) { return g && !g_running && !(g->flags & GF_UNKNOWN) && rom_available(g); }

/* the settings of the tabs as they were last saved / applied: Cancel and the X of a window put them back
   (ui_sig() reads the widgets into g_set all the time, so g_set alone cannot tell what was confirmed with OK) */
static Settings g_snap;
static int g_snapOk;
static void snap_take(void) { memcpy(&g_snap, &g_set, offsetof(Settings, onlyAvail)); g_snapOk = 1; }
static void snap_restore(void) { if (g_snapOk) memcpy(&g_set, &g_snap, offsetof(Settings, onlyAvail)); }

/* the key and pad columns of the mapping list from the bindings in memory */
static void prof_buttons(void);
static void bind_fill(void) {
    int i;
    wchar_t k[100], p[100];
    if (!W.lvBind) return;
    for (i = 0; i < NBIND; i++) {
        bind_text(i, k, p, 100);
        ListView_SetItemText(W.lvBind, i, 1, k);
        ListView_SetItemText(W.lvBind, i, 2, p);
        ListView_SetItemText(W.lvBind, i, 3, (LPWSTR)(bind_is_button(i) ? (bind_get_autofire(i) ? L"[x]" : L"[ ]") : L""));
    }
    prof_buttons();
}
/* the profile list: "None" first; the chosen one stays selected */
static wchar_t g_profNames[128][64];
static int g_nprof;
static int prof_changed(void) {   /* the list differs from the file of the chosen profile */
    int sel = (int)SendMessageW(W.cbProf, CB_GETCURSEL, 0, 0), diff = 1;
    std::string cur;
    char *t;
    if (sel <= 0 || sel > g_nprof) return 0;
    bind_to_text(cur);
    t = profile_load(g_profNames[sel - 1]);
    if (t) { diff = strcmp(t, cur.c_str()) != 0; free(t); }
    return diff;
}
static void prof_buttons(void) {
    int sel = (int)SendMessageW(W.cbProf, CB_GETCURSEL, 0, 0);
    if (!W.cbProf) return;
    EnableWindow(W.bPSave, sel > 0 && prof_changed()); EnableWindow(W.bPRen, sel > 0); EnableWindow(W.bPDel, sel > 0);
}
static void prof_fill(const wchar_t *keep) {
    int i, sel = 0;
    if (!W.cbProf) return;
    SendMessageW(W.cbProf, CB_RESETCONTENT, 0, 0);
    SendMessageW(W.cbProf, CB_ADDSTRING, 0, (LPARAM)L"None");
    g_nprof = profile_names(g_profNames, 128);
    for (i = 0; i < g_nprof; i++) { SendMessageW(W.cbProf, CB_ADDSTRING, 0, (LPARAM)g_profNames[i]); if (keep && !_wcsicmp(keep, g_profNames[i])) sel = i + 1; }
    SendMessageW(W.cbProf, CB_SETCURSEL, sel, 0);
    wcscpy(g_set.profile, sel ? g_profNames[sel - 1] : L"");
    prof_buttons();
}
static int prof_current(wchar_t *name) {   /* the chosen profile name; 0 for None */
    int sel = (int)SendMessageW(W.cbProf, CB_GETCURSEL, 0, 0);
    if (sel <= 0 || sel > g_nprof) return 0;
    wcscpy(name, g_profNames[sel - 1]);
    return 1;
}
static int prof_write(const wchar_t *name) {
    std::string t;
    bind_to_text(t);
    return profile_save(name, t.c_str());
}
static void prof_command(int id, int code) {
    wchar_t cur[64], nm[64];
    int have = prof_current(cur);
    if (id == ID_C_PROF) {
        if (code != CBN_SELCHANGE) return;
        wcscpy(g_set.profile, have ? cur : L"");
        if (have) {
            char *t = profile_load(cur);
            if (t) { bind_from_text(t); free(t); bind_fill(); set_note(L"Profile loaded: press OK to write it to MAME"); }
            else set_note(L"The profile could not be read");
        }
        prof_buttons();
        return;
    }
    if (id == ID_C_PSAVE) {
        if (!have) return;
        { int ok = prof_write(cur); prof_buttons(); set_note(ok ? L"Profile saved" : L"The profile could not be saved"); }
        return;
    }
    if (id == ID_C_PSAVEAS || id == ID_C_PREN) {
        int ren = id == ID_C_PREN;
        if (ren && !have) return;
        wcscpy(nm, ren ? cur : L"");
        if (!dlg_text(ui_owner(g_main), ren ? L"Rename Profile" : L"Save Profile As", L"Profile name:", nm, 64)) return;
        profile_clean_name(nm);
        if (!nm[0]) { set_note(L"Type a name for the profile"); return; }
        if (ren) {
            if (!_wcsicmp(nm, cur) && wcscmp(nm, cur) == 0) return;
            if (_wcsicmp(nm, cur) && profile_exists(nm)) { set_note(L"A profile with this name exists already"); return; }
            if (!profile_rename(cur, nm)) { set_note(L"The profile could not be renamed"); return; }
        } else {
            int r = IDYES;
            if (profile_exists(nm)) msg_box(g_main, L"Save Profile As", L"A profile with this name exists. Replace it?", MB_YESNO | MB_ICONQUESTION, &r);
            if (r != IDYES) return;
            if (!prof_write(nm)) { set_note(L"The profile could not be saved"); return; }
        }
        prof_fill(nm);
        set_note(ren ? L"Profile renamed" : L"Profile saved");
        return;
    }
    if (id == ID_C_PDEL) {
        int r = 0;
        if (!have) return;
        msg_box(g_main, L"Delete Profile", L"Delete this profile?\n\nThe mapping in the list stays as it is.", MB_YESNO | MB_ICONQUESTION, &r);
        if (r != IDYES) return;
        profile_delete(cur);
        prof_fill(NULL);
        set_note(L"Profile deleted");
    }
}
static int bind_selected(void) { return W.lvBind ? ListView_GetNextItem(W.lvBind, -1, LVNI_SELECTED) : -1; }

static void mo_apply_widgets(void);
static void mo_gather_widgets(void);

static int g_keepMaps;   /* Restore Defaults: the Controls and Combos tabs stay as they are */
static void apply_to_ui(void) {
    int i, sel = 0;
    g_applying = 1;
    if (!g_keepMaps) { cmb_load(); bind_load(); }   /* the mapping of the file (changes that were not confirmed with OK are dropped) */
    if (g_set.bindVer < 5) {   /* rows of older builds: completed once and written (and the new default keys) */
        int saved = 1;
        wchar_t exeChk[560];
        int mameOk = mame_exe(exeChk, 560);
        if (g_firstRun) {   /* the first start: the launcher's defaults, whatever cfg\default.cfg held before (a copy of it is kept) */
            if (mameOk) bind_backup_cfg();
            bind_reset_all();
            saved = mameOk ? bind_save() : 0;   /* without MAME the list shows them and OK writes them (into the folder of the MAME that is chosen then) */
        } else if (bind_migrate()) saved = bind_save();
        if (saved) { g_set.bindVer = 5; settings_save(); }   /* when the cfg could not be written the repair is tried again at the next start */
    }
    bind_fill();
    prof_fill(g_set.profile);
    SetWindowTextW(W.eMame, g_set.mame);
    SetWindowTextW(W.eRoms, g_set.roms);
    SetWindowTextW(W.eBios, g_set.biosDir);
    ctl_set_checked(W.chAvail, g_set.onlyAvail);
    ctl_set_checked(W.chHide, g_set.hideConsole);
    ctl_set_checked(W.chLogs, g_set.logs);
    set_combo(W.cbOnStart, g_set.onStart);
    set_combo(W.cbTheme, g_set.themeMode);
    ctl_set_checked(W.chClassic, g_set.optWindow == 0);
    /* video */
    set_combo(W.cbMode, g_set.video);
    ctl_set_checked(W.chFull, g_set.fullscreen);
    tb_set_checked(W.tbFs, g_set.fullscreen);
    for (i = 0; i < g_nmodes; i++) if (g_modes[i].w == g_set.resW && g_modes[i].h == g_set.resH) sel = i + 1;
    if (!sel && g_set.resW > 0 && g_set.resH > 0) {   /* a size of the settings that the list does not have is added to it */
        wchar_t t[40];
        g_modes = (Mode *)realloc(g_modes, (g_nmodes + 1) * sizeof(Mode));
        g_modes[g_nmodes].w = g_set.resW; g_modes[g_nmodes].h = g_set.resH; g_nmodes++;
        swprintf(t, 40, L"%d x %d", g_set.resW, g_set.resH);
        SendMessageW(W.cbRes, CB_ADDSTRING, 0, (LPARAM)t);
        sel = g_nmodes;
    }
    set_combo(W.cbRes, sel);
    set_combo(W.cbRot, g_set.rotate);
    ctl_set_checked(W.chAspect, g_set.keepAspect);
    ctl_set_checked(W.chUneven, g_set.unevenStretch);
    ctl_set_checked(W.chFilter, g_set.filter);
    mo_apply_widgets();
    set_combo(W.cbPrescale, g_set.prescale - 1);
    set_combo(W.cbIntScale, g_set.intScale);
    ctl_set_checked(W.chVsync, g_set.waitVsync);
    ctl_set_checked(W.chSyncRef, g_set.syncRefresh);
    ctl_set_checked(W.chThrottle, g_set.throttle);
    set_combo(W.cbFskip, g_set.frameskip < 0 ? 1 : g_set.frameskip == 0 ? 0 : g_set.frameskip + 1);
    ctl_set_checked(W.chMax, g_set.maximize);
    set_combo(W.cbChain, g_set.chain);
    set_combo(W.cbBackend, g_set.backend);
    ctl_set_checked(W.chCrop, g_set.artCrop);
    ctl_set_checked(W.chHlsl, g_set.hlsl);
    ctl_set_checked(W.chGlsl, g_set.glsl);
    ctl_set_checked(W.chFx, g_set.fxOn);
    set_combo(W.cbFxScaler, g_set.fxScaler);
    ctl_set_checked(W.chFxFxaa, g_set.fxFxaa);
    set_combo(W.cbFxDither, g_set.fxDither);
    set_combo(W.cbGlslF, g_set.glslFilter);
    SetWindowTextW(W.eGlslS, g_set.glslShader);
    /* audio */
    set_combo(W.cbSound, g_set.sound);
    for (i = 0, sel = 0; i < 5; i++) if (kSampleRates[i] == g_set.sampleRate) sel = i;
    set_combo(W.cbRate, sel);
    edit_set_int(W.eVol, -g_set.volume);
    set_combo(W.cbLat, g_set.latency - 1);
    /* controls */
    ctl_set_checked(W.chJoy, g_set.joystick);
    set_combo(W.cbProv, g_set.joyProvider);
    ctl_set_checked(W.chMouse, g_set.mouse);
    ctl_set_checked(W.chGun, g_set.lightgun);
    ctl_set_checked(W.chOvGame, g_set.ovGameCfg);
    set_combo(W.cbAf, g_set.afRate);
    ctl_set_checked(W.chContr, g_set.contradictory);
    edit_set_int(W.eDead, g_set.deadzone);
    edit_set_int(W.eSat, g_set.saturation);
    ctrlr_fill(g_set.ctrlr);
    /* advanced */
    ctl_set_checked(W.chSkip, g_set.skipInfo);
    ctl_set_checked(W.chSkipW, g_set.skipWarn);
    ctl_set_checked(W.chSave, g_set.autosave);
    ctl_set_checked(W.chCheat, g_set.cheat);
    ctl_set_checked(W.chQuit, g_set.confirmQuit);
    SetWindowTextW(W.eExtra, g_set.extra);
    ctl_set_checked(W.favOnly, g_set.favOnly);
    tb_set_checked(W.tbFav, g_set.favOnly);
    update_state();
    g_applying = 0;
    free(g_savedSig);
    g_savedSig = ui_sig();
    snap_take();
    update_buttons();
}

/* copies the widget values into the settings */
static void gather(void) {
    mo_gather_widgets();
    int i;
    GetWindowTextW(W.eMame, g_set.mame, 520); trim_w(g_set.mame);
    GetWindowTextW(W.eRoms, g_set.roms, 520); trim_w(g_set.roms);
    GetWindowTextW(W.eBios, g_set.biosDir, 520); trim_w(g_set.biosDir);
    g_set.onlyAvail = ctl_checked(W.chAvail);
    g_set.hideConsole = ctl_checked(W.chHide);
    g_set.logs = ctl_checked(W.chLogs);
    g_set.onStart = get_combo(W.cbOnStart);
    g_set.video = get_combo(W.cbMode);
    g_set.fullscreen = ctl_checked(W.chFull);
    i = get_combo(W.cbRes);
    if (i > 0 && i <= g_nmodes) { g_set.resW = g_modes[i - 1].w; g_set.resH = g_modes[i - 1].h; } else g_set.resW = g_set.resH = 0;
    g_set.rotate = get_combo(W.cbRot);
    g_set.keepAspect = ctl_checked(W.chAspect);
    g_set.unevenStretch = ctl_checked(W.chUneven);
    g_set.filter = ctl_checked(W.chFilter);
    g_set.prescale = get_combo(W.cbPrescale) + 1;
    g_set.intScale = get_combo(W.cbIntScale);
    g_set.waitVsync = ctl_checked(W.chVsync);
    g_set.syncRefresh = ctl_checked(W.chSyncRef);
    g_set.throttle = ctl_checked(W.chThrottle);
    i = get_combo(W.cbFskip);
    g_set.frameskip = i == 0 ? 0 : i == 1 ? -1 : i - 1;
    g_set.maximize = ctl_checked(W.chMax);
    g_set.chain = get_combo(W.cbChain);
    g_set.backend = get_combo(W.cbBackend);
    g_set.artCrop = ctl_checked(W.chCrop);
    g_set.hlsl = ctl_checked(W.chHlsl);
    g_set.glsl = ctl_checked(W.chGlsl);
    g_set.fxOn = ctl_checked(W.chFx);
    g_set.fxScaler = get_combo(W.cbFxScaler);
    g_set.fxFxaa = ctl_checked(W.chFxFxaa);
    g_set.fxDither = get_combo(W.cbFxDither);
    g_set.glslFilter = get_combo(W.cbGlslF);
    GetWindowTextW(W.eGlslS, g_set.glslShader, 64); trim_w(g_set.glslShader);
    g_set.sound = get_combo(W.cbSound);
    g_set.sampleRate = kSampleRates[get_combo(W.cbRate) % 5];
    g_set.volume = -edit_int(W.eVol, 0, 32);
    g_set.latency = get_combo(W.cbLat) + 1;
    g_set.joystick = ctl_checked(W.chJoy);
    g_set.joyProvider = get_combo(W.cbProv);
    g_set.mouse = ctl_checked(W.chMouse);
    g_set.lightgun = ctl_checked(W.chGun);
    g_set.ovGameCfg = ctl_checked(W.chOvGame);
    g_set.afRate = get_combo(W.cbAf);
    g_set.contradictory = ctl_checked(W.chContr);
    g_set.deadzone = edit_int(W.eDead, 0, 100);
    g_set.saturation = edit_int(W.eSat, 0, 100);
    i = get_combo(W.cbCtrlr);
    g_set.ctrlr[0] = 0;
    if (i > 0) {   /* the entry text: a file of the folder, or a name that is kept from the settings */
        wchar_t t[64];
        if (SendMessageW(W.cbCtrlr, CB_GETLBTEXTLEN, i, 0) < 64) { SendMessageW(W.cbCtrlr, CB_GETLBTEXT, i, (LPARAM)t); wcscpy(g_set.ctrlr, t); }
    }
    g_set.skipInfo = ctl_checked(W.chSkip);
    g_set.skipWarn = ctl_checked(W.chSkipW);
    g_set.autosave = ctl_checked(W.chSave);
    g_set.cheat = ctl_checked(W.chCheat);
    g_set.confirmQuit = ctl_checked(W.chQuit);
    GetWindowTextW(W.eExtra, g_set.extra, 520); trim_w(g_set.extra);
}

/* describes everything "OK" would write, to tell whether anything changed */
static char *ui_sig(void) {
    Buf b = {0};
    gather();
    settings_sig(&b);
    buf_fmt(&b, "classic=%d;", ctl_checked(W.chClassic));
    return b.s;
}

static void update_buttons(void) {
    char *s;
    if (g_applying) return;
    enable_ctl(W.iconBtn, selected_game() != NULL);
    EnableWindow(W.playBtn, playable(selected_game()));
    s = ui_sig();
    free(s);
}

static void set_optmode(int on, int show);

static int do_save(void) {
    int ok, want;
    gather();
    want = !ctl_checked(W.chClassic);   /* the options layout is applied with the other settings */
    g_set.optWindow = want;
    cmb_save();
    if (!bind_save()) set_note(L"Could not write the key mapping (MAME folder: cfg\\default.cfg)");
    ok = settings_save();
    if (!ok) set_note(L"Could not save the settings: the launcher folder is not writable");
    if (want != g_optWin) PostMessageW(g_main, WM_APP_LAYOUT, (WPARAM)want, 0);   /* after this message: the Save button itself moves */
    free(g_savedSig);
    g_savedSig = ui_sig();
    snap_take();
    update_buttons();
    return ok;
}

/* ---------------------------------------------------------------- playing */
static void rebuild_recent(void);

static DWORD WINAPI wait_thread(LPVOID h) {
    WaitForSingleObject((HANDLE)h, INFINITE);
    PostMessageW(g_main, WM_APP_GAMEEXIT, 0, 0);
    return 0;
}

static void launch_game_opts(const Game *g, const PlayOpts *po) {
    ArgList a;
    int i, n;
    IntList rec = {0};
    wchar_t exe[560];
    if (!g) { msg_box(g_main, L"Play", L"Select a game first.", MB_ICONINFORMATION, NULL); return; }
    if (g_running) { msg_box(g_main, L"Play", L"A game is already running.", MB_ICONINFORMATION, NULL); return; }
    if (g->flags & GF_UNKNOWN) { msg_box(g_main, L"Play", L"The installed MAME does not know this set (it is too old, or the set was renamed).", MB_ICONINFORMATION, NULL); return; }
    if (!rom_available(g)) { msg_box(g_main, L"Play", L"The ROM files of this game were not found in the ROMs folder.", MB_ICONINFORMATION, NULL); return; }
    if (!mame_exe(exe, 560)) {
        wchar_t t[700];
        swprintf(t, 700, L"mame.exe was not found:\n%ls\n\nChoose it on the General tab (MAME Program), or put it next to the launcher.", exe);
        msg_box(g_main, L"MAME Not Found", t, MB_ICONWARNING, NULL);
        return;
    }
    do_save();
    mame_args(g->id, po, &a);
    mame_log_start(&a);
    if (!mame_start(&a, &g_proc)) {
        wchar_t t[200];
        swprintf(t, 200, L"Windows error %lu", GetLastError());
        msg_box(g_main, L"Could Not Start MAME", t, MB_ICONERROR, NULL);
        args_free(&a);
        return;
    }
    args_free(&a);
    g_running = 1;
    g_stopped = 0;
    g_lastGameId = g->id;
    g_startTick = GetTickCount();
    {
        wchar_t t[400];
        swprintf(t, 400, L"Playing: %ls", g->title);
        set_status(t);
    }
    EnableWindow(W.stopBtn, TRUE);
    update_buttons();
    il_add(&rec, g->id);
    for (i = 0, n = 1; i < g_set.recent.n; i++) if (g_set.recent.v[i] != g->id && n < 8) { il_add(&rec, g_set.recent.v[i]); n++; }
    il_copy(&g_set.recent, &rec);
    il_clear(&rec);
    settings_save();
    rebuild_recent();
    CloseHandle(CreateThread(NULL, 0, wait_thread, g_proc.hProcess, 0, NULL));
    if (g_set.fxOn && !(po && po->noFilter)) filter_start(g_proc.dwProcessId);   /* the filter window, over the MAME window */
    if (g_set.onStart == 1) {   /* Minimize The Launcher: it comes back when the game ends */
        if (g_opt) hide_opt(0);
        ShowWindow(g_main, SW_MINIMIZE);
        g_minimized = 1;
    } else if (g_set.onStart == 2) {   /* Close The Launcher: the game goes on without it */
        PostMessageW(g_main, WM_CLOSE, 0, 0);
    }
}

static void launch_game(const Game *g) { launch_game_opts(g, NULL); }

static void launch_once(const Game *g, int fullscreen) {
    PlayOpts po;
    memset(&po, 0, sizeof po);
    po.fullscreen = fullscreen;
    launch_game_opts(g, &po);
}

static void rebuild_recent(void) {
    int i, n = 0, k;
    HMENU file = GetSubMenu(GetMenu(g_main), 0);
    while (GetMenuItemCount(g_recentMenu) > 0) DeleteMenu(g_recentMenu, 0, MF_BYPOSITION);
    for (i = 0; i < g_set.recent.n; i++) {
        for (k = 0; k < g_ngames; k++) {
            wchar_t label[600], *p;
            const Game *g = &g_games[k];
            size_t j;
            if (g->id != g_set.recent.v[i]) continue;
            n++;
            swprintf(label, 600, L"&%d  ", n);
            p = label + wcslen(label);
            for (j = 0; g->title[j] && p < label + 500; j++) { *p++ = g->title[j]; if (g->title[j] == L'&') *p++ = L'&'; }
            if (g->info[0]) {
                *p++ = L' '; *p++ = L' '; *p++ = L'[';
                for (j = 0; g->info[j] && p < label + 590; j++) { *p++ = g->info[j]; if (g->info[j] == L'&') *p++ = L'&'; }
                *p++ = L']';
            }
            *p = 0;
            AppendMenuW(g_recentMenu, MF_STRING, ID_RECENT0 + n - 1, label);
            break;
        }
    }
    if (!n) AppendMenuW(g_recentMenu, MF_STRING | MF_GRAYED, 0, L"(No Recent Games)");
    EnableMenuItem(file, ID_M_CLEARRECENT, MF_BYCOMMAND | (n ? MF_ENABLED : MF_GRAYED));
}

static void launch_recent(int slot) {
    int i, n = 0, k;
    for (i = 0; i < g_set.recent.n; i++)
        for (k = 0; k < g_ngames; k++)
            if (g_games[k].id == g_set.recent.v[i]) { if (n++ == slot) { launch_game(&g_games[k]); return; } break; }
}

typedef struct { Game *list; int n; int err; } FetchResult;
static DWORD WINAPI fetch_thread(LPVOID force) {
    FetchResult *r = (FetchResult *)calloc(1, sizeof *r);
    r->err = games_load(&r->list, &r->n);
    if (!r->err) games_ask_mame(r->list, r->n, (int)(INT_PTR)force);   /* which sets the installed MAME has: from the cache, unless forced */
    PostMessageW(g_main, WM_APP_GAMES, 0, (LPARAM)r);
    return 0;
}
/* force: ask MAME again (Rescan Games); at the start the answer of the last time is used while mame.exe is the same */
static void rescan(int force) {
    set_status(force ? L"Asking MAME for its games…" : L"Loading games…");
    CloseHandle(CreateThread(NULL, 0, fetch_thread, (LPVOID)(INT_PTR)force, 0, NULL));
}

static void open_folder(const wchar_t *dir) { ShellExecuteW(g_main, L"open", dir, NULL, NULL, SW_SHOWNORMAL); }
static wchar_t g_browseStart[560];
static int CALLBACK browse_cb(HWND h, UINT m, LPARAM l, LPARAM d) {   /* the folder dialog opens in g_browseStart */
    (void)l; (void)d;
    if (m == BFFM_INITIALIZED && g_browseStart[0]) SendMessageW(h, BFFM_SETSELECTIONW, TRUE, (LPARAM)g_browseStart);
    return 0;
}

static void browse_bios(void) {
    BROWSEINFOW bi;
    LPITEMIDLIST pidl;
    wchar_t path[MAX_PATH], cur[560];
    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = ui_owner(g_main);
    bi.lpszTitle = L"Select BIOS Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    GetWindowTextW(W.eBios, cur, 560); trim_w(cur);
    if (!cur[0]) roms_dir_abs(cur, 560);
    if (is_abs_path(cur)) wcsncpy(g_browseStart, cur, 559); else path_join(g_browseStart, 560, g_root, cur);
    g_browseStart[559] = 0;
    if (GetFileAttributesW(g_browseStart) == INVALID_FILE_ATTRIBUTES) path_join(g_browseStart, 560, g_root, L"roms");
    bi.lpfn = browse_cb;
    CoInitialize(NULL);
    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (SHGetPathFromIDListW(pidl, path)) { SetWindowTextW(W.eBios, path); refilter(); }
        CoTaskMemFree(pidl);
    }
}

static void browse_roms(void) {
    BROWSEINFOW bi;
    LPITEMIDLIST pidl;
    wchar_t path[MAX_PATH];
    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = ui_owner(g_main);
    bi.lpszTitle = L"Select ROMs Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    {   /* it starts in the ROMs folder of the settings (./roms by default) */
        wchar_t cur[560];
        GetWindowTextW(W.eRoms, cur, 560); trim_w(cur);
        { wchar_t *semi = wcschr(cur, L';'); if (semi) *semi = 0; }   /* the first of several folders */
        if (!cur[0]) roms_dir_abs(cur, 560);
        if (is_abs_path(cur)) wcsncpy(g_browseStart, cur, 559); else path_join(g_browseStart, 560, g_root, cur);
        g_browseStart[559] = 0;
        if (GetFileAttributesW(g_browseStart) == INVALID_FILE_ATTRIBUTES) path_join(g_browseStart, 560, g_root, L"roms");
    }
    bi.lpfn = browse_cb;
    CoInitialize(NULL);
    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (SHGetPathFromIDListW(pidl, path)) {
            wchar_t all[1100];
            GetWindowTextW(W.eRoms, all, 520); trim_w(all);
            if ((GetKeyState(VK_CONTROL) & 0x8000) && all[0]) { wcscat(all, L";"); wcsncat(all, path, 520); SetWindowTextW(W.eRoms, all); }   /* Ctrl: one more folder */
            else SetWindowTextW(W.eRoms, path);
            refilter();
        }
        CoTaskMemFree(pidl);
    }
}

static void desktop_icon(void) {
    const Game *g = selected_game();
    wchar_t err[400];
    if (!g) { msg_box(g_main, L"Desktop Icon", L"Select a game first.", MB_ICONINFORMATION, NULL); return; }
    do_save();
    if (!create_desktop_icon(g, err, 400)) {
        wchar_t t[500];
        swprintf(t, 500, L"Could not create the shortcut:\n%ls", err);
        msg_box(g_main, L"Desktop Icon", t, MB_ICONWARNING, NULL);
        return;
    }
    {
        wchar_t t[400];
        swprintf(t, 400, L"Desktop icon created: %ls", g->title);
        set_status(t);
    }
}

static void toggle_fav(void) {
    const Game *g = selected_game();
    if (!g) return;
    set_favorite(g->id, !il_has(&g_set.fav, g->id));
    refilter();
}

/* ---- backup, restore, MAME program */
static int pick_zip(int save, wchar_t *file, int cap) {
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = ui_owner(g_main);
    ofn.lpstrFilter = L"Zip files (*.zip)\0*.zip\0All files\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = (DWORD)cap;
    ofn.lpstrDefExt = L"zip";
    ofn.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST) : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
    return save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
}

static void do_backup(void) {
    wchar_t file[560];
    SYSTEMTIME st;
    int n;
    do_save();
    GetLocalTime(&st);
    swprintf(file, 560, L"MAME-EX-backup-%04d-%02d-%02d.zip", st.wYear, st.wMonth, st.wDay);
    if (!pick_zip(1, file, 560)) return;
    n = backup_create(file);
    if (n > 0) { wchar_t t[700]; swprintf(t, 700, L"Backup saved: %d files in %ls", n, file); set_note(t); }
    else msg_box(g_main, L"Backup Settings", L"The backup could not be written.", MB_ICONWARNING, NULL);
}

static void do_restore(void) {
    wchar_t file[560] = L"", keep[560];
    int r = 0, n;
    if (g_running) { msg_box(g_main, L"Restore Settings", L"Stop the running game first.", MB_ICONINFORMATION, NULL); return; }
    if (!pick_zip(0, file, 560)) return;
    msg_box(g_main, L"Restore Settings",
            L"Replace the current settings, per-game settings and the MAME saves (input mapping, nvram, save states) with the ones in this backup?\n\nThe current ones are saved first as MAME-EX-before-restore.zip in the launcher folder.",
            MB_YESNO | MB_ICONQUESTION, &r);
    if (r != IDYES) return;
    do_save();
    path_join(keep, 560, g_root, L"MAME-EX-before-restore.zip");
    backup_create(keep);
    n = backup_restore(file);
    if (n < 0) { msg_box(g_main, L"Restore Settings", L"This file is not a readable backup of MAME EX (damaged or not a zip).", MB_ICONWARNING, NULL); return; }
    settings_defaults(&g_set, NULL);   /* then everything that the backup holds */
    settings_load();
    gamecfg_load();
    apply_to_ui();
    fill_filters();
    refilter();
    { wchar_t t[200]; swprintf(t, 200, L"Backup restored: %d files", n); set_note(t); }
}

static void browse_mame(void) {
    OPENFILENAMEW ofn;
    wchar_t file[560];
    GetWindowTextW(W.eMame, file, 560); trim_w(file);
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = ui_owner(g_main);
    ofn.lpstrFilter = L"MAME (mame*.exe)\0mame*.exe\0Programs (*.exe)\0*.exe\0All files\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 560;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) { SetWindowTextW(W.eMame, file); ctrlr_fill(g_set.ctrlr); }
}

/* Game > Game Settings: the video mode, window, sound, cheats ... of the selected game only */
static void game_settings(void) {
    const Game *g = selected_game();
    if (!g) { msg_box(g_main, L"Game Settings", L"Select a game first.", MB_ICONINFORMATION, NULL); return; }
    if (dlg_game_settings(g_main, g)) {
        set_note(gamecfg_find(g->id) ? L"This game now has its own settings" : L"This game follows the main settings again");
        selection_status();
    }
}

static void default_settings(void) {
    int r = 0;
    msg_box(g_main, L"Default Settings",
            L"Restore the Video, Audio and General options (including Skip Game Info / Warning Screen, Auto Save, Cheats, Confirm Quit and Extra Arguments) to their defaults (MAME's own defaults)?\n\nKept as they are: the MAME program, the ROMs folder, the favorites, the window layout, and the Controls and Combos tabs. The defaults are saved right away.",
            MB_YESNO | MB_ICONQUESTION, &r);
    if (r != IDYES) return;
    gather();   /* what was typed in the paths stays */
    settings_defaults(&g_set, &g_set);
    g_keepMaps = 1;
    apply_to_ui();
    g_keepMaps = 0;
    if (do_save()) set_note(L"Default settings restored and saved");
}

/* ---------------------------------------------------------------- layout */
static void place(HWND h, int x, int y, int w, int hh) { SetWindowPos(h, NULL, x, y, w, hh, SWP_NOZORDER | SWP_NOACTIVATE); }

/* the height a note needs at a given width (its text is wrapped) */
static int note_height(HWND l, int w) {
    wchar_t buf[2000];
    RECT rc = {0, 0, w, 0};
    HDC dc;
    HGDIOBJ old;
    GetWindowTextW(l, buf, 2000);
    dc = GetDC(l);
    old = SelectObject(dc, g_font);
    DrawTextW(dc, buf, -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    ReleaseDC(l, dc);
    return rc.bottom + S(8);
}

static int layout_rows(PRow *r, int n, int w, int h, int off) {   /* returns the height the rows need; off: how far the page is scrolled */
    int x = S(10), y = S(10) - off, lw = 0, i, W2 = w - 2 * S(10), step = S(23);
    if (n > 20 && h > 0) {   /* the long video page: the rows are spread evenly over the height of the page */
        int rows = 0, lines = 0, hdrs = 0;
        for (i = 0; i < n; i++) {
            if (r[i].kind == 2) { lines++; if (i + 1 < n && r[i + 1].kind == 2) i++; }
            else if (r[i].kind == 5) hdrs++;
            else rows++;
        }
        if (rows) {
            step = (h - 2 * S(10) - lines * S(24) - hdrs * S(32) - S(8)) / rows;
            if (step < S(23)) step = S(23);
            if (step > S(32)) step = S(32);
        }
    }
    for (i = 0; i < n; i++)
        if (r[i].kind < 2) { wchar_t b[120]; int t; GetWindowTextW(r[i].lbl, b, 120); t = text_width(g_main, b); if (t > lw) lw = t; }
    lw += S(14);
    if (lw > W2 / 2) lw = W2 / 2;
    for (i = 0; i < n; i++) {
        if (r[i].kind == 2 && i + 2 < n && r[i + 1].kind == 11 && r[i + 2].kind == 11) {   /* check box, then two labelled lists on one row */
            wchar_t b[120]; int g = S(8), cw, l1, l2, cb;
            GetWindowTextW(r[i].ctl, b, 120); cw = text_width(g_main, b) + S(30);
            GetWindowTextW(r[i + 1].lbl, b, 120); l1 = text_width(g_main, b) + S(10);
            GetWindowTextW(r[i + 2].lbl, b, 120); l2 = text_width(g_main, b) + S(10);
            cb = (W2 - cw - l1 - l2 - 3 * g) / 2;
            if (cb < S(60)) cb = S(60);
            place(r[i].ctl, x, y + S(1), cw, S(21));
            place(r[i + 1].lbl, x + cw + g, y + S(4), l1, S(18)); place(r[i + 1].ctl, x + cw + g + l1, y, cb, S(23));
            place(r[i + 2].lbl, x + cw + 2 * g + l1 + cb, y + S(4), l2, S(18)); place(r[i + 2].ctl, x + cw + 2 * g + l1 + l2 + cb, y, W2 - (cw + 2 * g + l1 + l2 + cb), S(23));
            i += 2; y += n > 20 ? step : S(27); continue;
        }
        if (r[i].kind == 2) {
            if (n > 20 && i > 0 && r[i - 1].kind != 2 && r[i - 1].kind != 5) y += S(8);   /* a little gap before the check boxes */
            if (n > 20 && i + 1 < n && r[i + 1].kind == 2) {   /* the long video page: check boxes side by side */
                place(r[i].ctl, x, y, W2 / 2, S(21)); place(r[i + 1].ctl, x + W2 / 2, y, W2 / 2, S(21));
                i++; y += S(24); continue;
            }
            place(r[i].ctl, x, y, W2, S(21)); y += S(24); continue;
        }
        if (r[i].kind == 3) { place(r[i].ctl, x, y, W2, S(26)); y += S(32); continue; }
        if (r[i].kind == 7) {   /* the list: all the room that is left (the rows after it keep theirs) */
            int lh = h - y - (n - 1 - i) * S(34) - S(10);
            if (lh < S(140)) lh = S(140);
            place(r[i].ctl, x, y, W2, lh);
            y += lh + S(6);
            continue;
        }
        if (r[i].kind == 8) {   /* four buttons side by side */
            int g = S(6), bw = (W2 - 3 * g) / 4;
            place(r[i].lbl, x, y, bw, S(26)); place(r[i].ctl, x + bw + g, y, bw, S(26));
            place(r[i].btn, x + 2 * (bw + g), y, bw, S(26)); place(r[i].clr, x + 3 * (bw + g), y, bw, S(26));
            y += S(32);
            continue;
        }
        if (r[i].kind == 6) { int nh = note_height(r[i].lbl, W2); place(r[i].lbl, x, y, W2, nh); y += nh + S(8); continue; }   /* a note: as high as its wrapped text */
        if (r[i].kind == 5) {   /* a group title: the text, then a line */
            wchar_t b[120]; int tw;
            GetWindowTextW(r[i].lbl, b, 120); tw = text_width(g_main, b) + S(10);
            y += S(8);
            place(r[i].lbl, x, y, tw, S(18));
            place(r[i].btn, x + tw, y + S(9), W2 - tw, S(1) > 1 ? S(1) : 1);
            y += S(24); continue;
        }
        place(r[i].lbl, x, y + S(4), lw, S(18));
        if (i + 1 < n && r[i + 1].kind == 4) {   /* an edit with a second label + edit (Width, Height) on the same row */
            wchar_t b[120]; int hl, g = S(8), ew;
            GetWindowTextW(r[i + 1].lbl, b, 120); hl = text_width(g_main, b) + S(14);
            ew = (W2 - lw - hl - g) / 2;
            if (ew < S(50)) ew = S(50);
            place(r[i].ctl, x + lw, y, ew, S(n > 20 ? 22 : 23));
            place(r[i + 1].lbl, x + lw + ew + g, y + S(4), hl, S(18));
            place(r[i + 1].ctl, x + lw + ew + g + hl, y, W2 - lw - ew - g - hl, S(n > 20 ? 22 : 23));
            i++; y += n > 20 ? step : S(27); continue;
        }
        if (r[i].btn) {   /* an edit with Browse and a small X on the same row */
            int bw = S(80), xw = S(26), g = S(4), ew = W2 - lw - bw - xw - 2 * g;
            if (ew < S(60)) ew = S(60);
            place(r[i].ctl, x + lw, y, ew, S(23));
            place(r[i].btn, x + lw + ew + g, y - 1, bw, S(25));
            place(r[i].clr, x + lw + ew + g + bw + g, y - 1, xw, S(25));
            y += S(27);
            continue;
        }
        place(r[i].ctl, x + lw, y, W2 - lw, r[i].kind == 0 ? S(220) : n > 20 ? S(22) : S(23));
        y += n > 20 ? step : S(27);
    }
    return y + off + S(10);
}

/* The pages that can be longer than the window (General, Video): when the rows do not fit the page gets a scroll bar. */
static struct { PRow *rows; int n, w, h; } g_sp[NPAGES];
static int g_scroll[NPAGES];

static void layout_scroll_page(int pg, PRow *rows, int n, int w, int h) {
    HWND page = W.page[pg];
    int total = layout_rows(rows, n, w, h, 0), w2 = w;
    g_sp[pg].rows = rows; g_sp[pg].n = n; g_sp[pg].h = h;
    if (total > h) {   /* the bar takes some width: lay out again with the narrower page */
        SCROLLINFO si;
        int maxPos;
        w2 = w - GetSystemMetrics(SM_CXVSCROLL);
        total = layout_rows(rows, n, w2, h, 0);
        maxPos = total - h > 0 ? total - h : 0;
        if (g_scroll[pg] > maxPos) g_scroll[pg] = maxPos;
        if (g_scroll[pg] < 0) g_scroll[pg] = 0;
        memset(&si, 0, sizeof si);
        si.cbSize = sizeof si; si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = total - 1; si.nPage = (UINT)h; si.nPos = g_scroll[pg];
        SetScrollInfo(page, SB_VERT, &si, TRUE);
        ShowScrollBar(page, SB_VERT, TRUE);
        layout_rows(rows, n, w2, h, g_scroll[pg]);
    } else {
        g_scroll[pg] = 0;
        ShowScrollBar(page, SB_VERT, FALSE);
    }
    g_sp[pg].w = w2;
}

static void scroll_page_to(int pg, int pos) {
    SCROLLINFO si;
    int maxPos;
    memset(&si, 0, sizeof si);
    si.cbSize = sizeof si; si.fMask = SIF_ALL;
    GetScrollInfo(W.page[pg], SB_VERT, &si);
    maxPos = si.nMax - (int)si.nPage + 1;
    if (maxPos < 0) maxPos = 0;
    if (pos > maxPos) pos = maxPos;
    if (pos < 0) pos = 0;
    if (pos == g_scroll[pg]) return;
    {   /* Windows moves the child windows and copies the pixels that stay: no tearing of the texts */
        int delta = g_scroll[pg] - pos;
        g_scroll[pg] = pos;
        si.fMask = SIF_POS; si.nPos = pos;
        SetScrollInfo(W.page[pg], SB_VERT, &si, TRUE);
        ScrollWindowEx(W.page[pg], 0, delta, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
        UpdateWindow(W.page[pg]);
    }
}

static void size_columns(void);

/* back to the default window: size, centered position, splitter and column widths */
static void reset_window(int keepColumns) {
    RECT wa;
    int w = g_optWin ? S(860) : S(1340), h = g_optWin ? S(720) : S(800), x, y, ow = S(690), oh = S(800), gap = S(8);
    if (IsZoomed(g_main) || IsIconic(g_main)) ShowWindow(g_main, SW_RESTORE);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (w > wa.right - wa.left) w = wa.right - wa.left;
    if (h > wa.bottom - wa.top) h = wa.bottom - wa.top;
    if (oh > wa.bottom - wa.top) oh = wa.bottom - wa.top;
    x = wa.left + (wa.right - wa.left - w) / 2;
    y = wa.top + (wa.bottom - wa.top - h) / 2;
    if (g_optWin) {   /* the options are in a window of their own: the main window is narrower, and the options window stands next to it when both fit */
        if (x + w + gap + ow > wa.right) ow = wa.right - x - w - gap > S(660) ? wa.right - x - w - gap : ow;   /* the main window stays centered; the settings window is beside it */
        g_optRect.w = ow; g_optRect.h = oh;
        g_optRect.x = x + w + gap; g_optRect.y = wa.top + (wa.bottom - wa.top - oh) / 2;
        if (g_optRect.x + g_optRect.w > wa.right) g_optRect.x = wa.right - g_optRect.w;
        if (g_opt) SetWindowPos(g_opt, NULL, g_optRect.x, g_optRect.y, g_optRect.w, g_optRect.h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (!keepColumns) {
        g_splitX = 0;
        g_columnsSized = 0;
        memset(g_set.colsGame, 0, sizeof g_set.colsGame); g_set.splitX = 0;   /* "Reset Window Size": the default columns and splitter as well */
    }
    SetWindowPos(g_main, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    size_columns();
    RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    g_winRect.x = x; g_winRect.y = y; g_winRect.w = w; g_winRect.h = h;
}

/* the status bar tells which ROM files of the selected game are missing */
static void selection_status(void) {
    const Game *g;
    wchar_t miss[300], t[400];
    if (g_running) return;
    g = selected_game();
    if (g && !rom_available(g) && rom_missing(g, miss, 300)) {
        swprintf(t, 400, L"Missing in the ROMs folder: %ls", miss);
        set_status(t);
    } else {
        games_status();
        if (g && gamecfg_find(g->id)) { get_status(t, 400); wcscat(t, L"   |   This game has its own settings (Game > Game Settings)"); set_status(t); }
    }
}

/* View: the search bar and the filters bar of the game list can be hidden */
static void apply_bars(void) {
    int s = SEARCH_SHOWN() ? SW_SHOW : SW_HIDE, f = g_set.hideFilters ? SW_HIDE : SW_SHOW;
    {   /* the toolbar */
        int t = g_set.hideToolbar ? SW_HIDE : SW_SHOW;
        HWND tb[] = {W.tbRefresh, W.tbFav, W.tbAvail, W.tbSep1, W.playBtn, W.stopBtn, W.tbFs, W.tbSep2, W.tbSet, W.tbVideo, W.tbAudio, W.tbCtl, W.tbAdv};
        int k;
        for (k = 0; k < (int)(sizeof tb / sizeof tb[0]); k++) ShowWindow(tb[k], t);
    }
    ShowWindow(W.lblSearch, s); ShowWindow(W.search, s); ShowWindow(W.clearBtn, s);
    ShowWindow(W.cbRegion, f); ShowWindow(W.cbMaker, f); ShowWindow(W.cbGenre, f); ShowWindow(W.cbHw, f); ShowWindow(W.sortBtn, f);
}

/* the tab control with its pages and the save button, wherever they are (main window or options window): tab rectangle tx ty tw th, save button at sx sy */
static void layout_tabs(int tx, int ty, int tw, int th, int sx, int sy) {
    RECT tr;
    int i, w, h;
    place(W.tab, tx, ty, tw, th);
    GetClientRect(W.tab, &tr);
    TabCtrl_AdjustRect(W.tab, FALSE, &tr);
    tr.left += tx; tr.right += tx; tr.top += ty; tr.bottom += ty;
    w = tr.right - tr.left; h = tr.bottom - tr.top;
    for (i = 0; i < NPAGES; i++) place(W.page[i], tr.left, tr.top, w, h);
    layout_scroll_page(PG_VIDEO, W.vrows, W.nv, w, h);
    layout_rows(W.arows, W.na, w, 0, 0);
    layout_scroll_page(PG_SYSTEM, W.srows, W.ns, w, h);
    layout_scroll_page(PG_CONTROLS, W.crows, W.nc, w, h);
    cmb_layout(w, h);
    place(W.defBtn, tx, sy, S(120), S(26));   /* Restore Defaults: opposite of OK */
    place(W.cancelBtn, sx + S(120) - S(80), sy, S(80), S(26));   /* OK, Cancel: at the right bottom */
    place(W.saveBtn, sx + S(120) - 2 * S(80) - S(8), sy, S(80), S(26));
}

static void layout_opt(void);

static void do_layout(void) {
    RECT rc, tr;
    int cw, ch, sbh, m = S(8), gap = S(8), x, y, w, h, leftW, rightX, rightW, i, bh = S(26), by, ly;
    GetClientRect(g_main, &rc);
    SendMessageW(W.status, WM_SIZE, 0, 0);
    GetWindowRect(W.status, &tr);
    sbh = tr.bottom - tr.top;
    cw = rc.right; ch = rc.bottom - sbh;
    if (g_splitX <= 0 && g_set.splitX > 0) g_splitX = S(g_set.splitX);   /* the splitter of the last session */
    if (g_splitX <= 0) g_splitX = (cw - 2 * m - gap) - S(660) > S(360) ? (cw - 2 * m - gap) - S(660) : (cw - 2 * m - gap) * 3 / 5;   /* the tabs get 660 pixels (the Combos tab needs them), the list the rest */
    leftW = g_splitX;
    if (leftW < S(600)) leftW = S(600);   /* the toolbar is over the list */
    if (leftW > cw - 2 * m - gap - S(300)) leftW = cw - 2 * m - gap - S(300);
    g_splitX = leftW;
    if (g_optWin) leftW = cw - 2 * m;   /* the tabs are in the options window: the list has the whole width */
    {   /* the toolbar: Refresh, Favorites Only | Play, Stop, Fullscreen | Settings, Video, Audio, Controls, Combos */
        int bx = m, ty = m, bw = S(38), bh2 = S(32), g2 = S(2), sp = S(16);
        place(W.tbRefresh, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbFav, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbAvail, bx, ty, bw, bh2); bx += bw;
        place(W.tbSep1, bx, ty, sp, bh2); bx += sp;
        place(W.playBtn, bx, ty, bw, bh2); bx += bw + g2;
        place(W.stopBtn, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbFs, bx, ty, bw, bh2); bx += bw;
        place(W.tbSep2, bx, ty, sp, bh2); bx += sp;
        place(W.tbSet, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbVideo, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbAudio, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbCtl, bx, ty, bw, bh2); bx += bw + g2;
        place(W.tbAdv, bx, ty, bw, bh2);
    }
    x = m; y = m + (g_set.hideToolbar ? 0 : S(40));
    /* left: search row, list, buttons */
    ly = y;   /* top of the list: below the search bar and the filters bar when they are shown */
    if (SEARCH_SHOWN()) {
        place(W.lblSearch, x, ly + S(4), S(52), S(18));
        place(W.search, x + S(54), ly, leftW - S(54) - S(96), S(24));
        place(W.clearBtn, x + leftW - S(90), ly - 1, S(90), S(26));
        ly += S(32);
    }
    if (!g_set.hideFilters) {
        int cw = (leftW - S(96) - 3 * S(6)) / 4;
        place(W.cbHw, x, ly, cw, S(220));
        place(W.cbRegion, x + (cw + S(6)), ly, cw, S(220));
        place(W.cbMaker, x + 2 * (cw + S(6)), ly, cw, S(220));
        place(W.cbGenre, x + 3 * (cw + S(6)), ly, cw, S(220));
        place(W.sortBtn, x + leftW - S(90), ly - S(1), S(90), S(26));   /* Reset Filters */
        ly += S(30);
    }
    place(W.lv, x, ly, leftW, ch - m - ly);
    /* right: tabs, default / save buttons (in the options window instead: layout_opt) */
    if (!g_optWin) {
        rightX = x + leftW + gap;
        rightW = cw - m - rightX;
        /* the toolbar is over the game list only: the tabs have the whole height, Save Settings is at the bottom */
        layout_tabs(rightX, m, rightW, ch - 2 * m - bh - S(6), rightX + rightW - S(120), ch - m - bh);
    }
}

/* the options window: the tabs fill it */
static void layout_opt(void) {
    RECT rc;
    int m = S(8), bh = S(26);
    if (!g_opt) return;
    GetClientRect(g_opt, &rc);
    layout_tabs(m, m, rc.right - 2 * m, rc.bottom - 2 * m - bh - S(6), rc.right - m - S(120), rc.bottom - m - bh);
}

/* ---------------------------------------------------------------- creating the window contents */
#define TIP(h, key) set_tip(W.tip, GetParent(h), h, tip_for(key))

static HWND add_label(HWND p, const wchar_t *t) { HWND l = mk(p, L"STATIC", t, SS_LEFT, 0, 0); TIP(l, t); return l; }

static HWND add_combo(HWND p, PRow *rows, int *n, const wchar_t *label, int id, const wchar_t *const *items, int ni) {
    HWND l = add_label(p, label), c = mk(p, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, id);
    if (items) { combo_fill(c, (const wchar_t **)items, ni); SendMessageW(c, CB_SETCURSEL, 0, 0); }
    TIP(c, label);
    SetPropW(c, L"lbl", (HANDLE)l);
    rows[*n].lbl = l; rows[*n].ctl = c; rows[*n].kind = 0; (*n)++;
    return c;
}
static HWND add_edit(HWND p, PRow *rows, int *n, const wchar_t *label, int id, int numeric) {
    HWND l = add_label(p, label), e = mk(p, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | (numeric ? ES_NUMBER : 0), WS_EX_CLIENTEDGE, id);
    TIP(e, label);
    SetPropW(e, L"lbl", (HANDLE)l);
    rows[*n].lbl = l; rows[*n].ctl = e; rows[*n].kind = 1; (*n)++;
    return e;
}
static HWND add_check(HWND p, PRow *rows, int *n, const wchar_t *label, int id) {
    HWND c = mk(p, L"BUTTON", label, BS_AUTOCHECKBOX | WS_TABSTOP, 0, id);
    TIP(c, label);
    rows[*n].lbl = NULL; rows[*n].ctl = c; rows[*n].kind = 2; (*n)++;
    return c;
}

/* controls that show one option of the All MAME Options store (Display, Capture And Speed, Light Gun ...) */
typedef struct { HWND w; const char *opt; char kind; const char *const *vals; int nvals; } MoW;   /* kind: e edit, c check box, b combo box (values vals) */
static MoW g_mow[40];
static int g_nmow;
static HWND mo_edit_row(HWND p, PRow *rows, int *n, const wchar_t *label, const char *opt) {
    HWND e = add_edit(p, rows, n, label, 0, 0);
    g_mow[g_nmow].w = e; g_mow[g_nmow].opt = opt; g_mow[g_nmow].kind = 'e'; g_nmow++;
    return e;
}
static HWND mo_check_row(HWND p, PRow *rows, int *n, const wchar_t *label, const char *opt) {
    HWND c = add_check(p, rows, n, label, 0);
    g_mow[g_nmow].w = c; g_mow[g_nmow].opt = opt; g_mow[g_nmow].kind = 'c'; g_nmow++;
    return c;
}
static HWND mo_combo_row(HWND p, PRow *rows, int *n, const wchar_t *label, const char *opt, const wchar_t *const *items, const char *const *vals, int count) {
    HWND c = add_combo(p, rows, n, label, 0, items, count);
    g_mow[g_nmow].w = c; g_mow[g_nmow].opt = opt; g_mow[g_nmow].kind = 'b'; g_mow[g_nmow].vals = vals; g_mow[g_nmow].nvals = count; g_nmow++;
    return c;
}
static void mo_apply_widgets(void) {
    int i, k;
    for (i = 0; i < g_nmow; i++) {
        std::wstring v = mo_get(g_mow[i].opt);
        if (g_mow[i].kind == 'e') SetWindowTextW(g_mow[i].w, v.c_str());
        else if (g_mow[i].kind == 'c') ctl_set_checked(g_mow[i].w, v == L"1");
        else {
            int sel = 0;
            for (k = 0; k < g_mow[i].nvals; k++) { wchar_t *w = u8_to_w(g_mow[i].vals[k], -1); if (v == w) sel = k; free(w); }
            set_combo(g_mow[i].w, sel);
        }
    }
}
static void mo_gather_widgets(void) {
    int i;
    for (i = 0; i < g_nmow; i++) {
        if (g_mow[i].kind == 'e') { wchar_t t[260]; GetWindowTextW(g_mow[i].w, t, 260); mo_set(g_mow[i].opt, t); }   /* an invalid text is ignored (the old value stays) */
        else if (g_mow[i].kind == 'c') mo_set(g_mow[i].opt, ctl_checked(g_mow[i].w) ? L"1" : L"0");
        else { int k = get_combo(g_mow[i].w); wchar_t *w = u8_to_w(g_mow[i].vals[k < 0 ? 0 : k % g_mow[i].nvals], -1); mo_set(g_mow[i].opt, w); free(w); }
    }
}

/* a title of a group of settings, with a line to the right of it */
static void add_header(HWND p, PRow *rows, int *n, const wchar_t *text) {
    HWND l = mk(p, L"STATIC", text, SS_LEFT, 0, 0), ln = mk(p, L"STATIC", L"", SS_LEFT, 0, 0);
    SetPropW(ln, L"hline", (HANDLE)1);   /* a thin line: painted in a grey of the theme (see page_proc) */
    rows[*n].lbl = l; rows[*n].ctl = NULL; rows[*n].btn = ln; rows[*n].clr = NULL; rows[*n].kind = 5; (*n)++;
}

/* a few lines of explanation */
static void add_note(HWND p, PRow *rows, int *n, const wchar_t *text) {
    HWND l = mk(p, L"STATIC", text, SS_LEFT, 0, 0);
    rows[*n].lbl = l; rows[*n].ctl = NULL; rows[*n].btn = NULL; rows[*n].clr = NULL; rows[*n].kind = 6; (*n)++;
}

static LRESULT CALLBACK page_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_COMMAND: return SendMessageW(g_main, WM_COMMAND, w, l);
    case WM_NOTIFY: return SendMessageW(g_main, WM_NOTIFY, w, l);
    case WM_VSCROLL: case WM_MOUSEWHEEL: {   /* the scroll bar of a long page (General, Video) */
        int pg, found = -1;
        for (pg = 0; pg < NPAGES; pg++) if (W.page[pg] == h) found = pg;
        if (found >= 0 && g_sp[found].rows) {
            SCROLLINFO si;
            int pos;
            memset(&si, 0, sizeof si);
            si.cbSize = sizeof si; si.fMask = SIF_ALL;
            GetScrollInfo(h, SB_VERT, &si);
            pos = si.nPos;
            if (m == WM_MOUSEWHEEL) pos -= (GET_WHEEL_DELTA_WPARAM(w) / 120) * S(46);
            else switch (LOWORD(w)) {
            case SB_LINEUP: pos -= S(23); break;
            case SB_LINEDOWN: pos += S(23); break;
            case SB_PAGEUP: pos -= (int)si.nPage; break;
            case SB_PAGEDOWN: pos += (int)si.nPage; break;
            case SB_TOP: pos = 0; break;
            case SB_BOTTOM: pos = si.nMax; break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
            }
            scroll_page_to(found, pos);
        }
        return 0;
    }
    /* the tab body is plain window colour (white) in current Windows versions: pages and their labels use the same colour */
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(h, &rc);
        FillRect((HDC)w, &rc, th_page_brush());
        return 1;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (g_dark) return (LRESULT)th_ctlcolor(m, (HDC)w, (HWND)l, 1);
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (m == WM_CTLCOLORSTATIC && GetPropW((HWND)l, L"hline")) {   /* the line of a group title */
            static HBRUSH bd, bl;
            if (!bd) { bd = CreateSolidBrush(RGB(80, 80, 80)); bl = CreateSolidBrush(RGB(200, 200, 200)); }
            return (LRESULT)(g_dark ? bd : bl);
        }
        if (g_dark) return (LRESULT)th_ctlcolor(m, (HDC)w, (HWND)l, 1);
        if (m == WM_CTLCOLORSTATIC && (GetPropW((HWND)l, L"dim") || !IsWindowEnabled((HWND)l))) SetTextColor((HDC)w, GetSysColor(COLOR_GRAYTEXT));   /* a label of a control that does not apply */
        SetBkColor((HDC)w, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    return DefDlgProcW(h, m, w, l);
}

static int on_screen(int x, int y, int w, int h);
static void select_tab(void);

/* ---------------------------------------------------------------- the options window */
static void remember_opt_rect(void) {
    RECT r;
    if (g_opt && IsWindowVisible(g_opt) && !IsIconic(g_opt) && GetWindowRect(g_opt, &r) && r.right - r.left > 200 && r.bottom - r.top > 200) {
        g_optRect.x = r.left; g_optRect.y = r.top; g_optRect.w = r.right - r.left; g_optRect.h = r.bottom - r.top;
    }
}

static LRESULT CALLBACK opt_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_COMMAND: return SendMessageW(g_main, WM_COMMAND, w, l);
    case WM_NOTIFY: return SendMessageW(g_main, WM_NOTIFY, w, l);
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) { layout_opt(); RedrawWindow(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN); }
        return 0;
    case WM_EXITSIZEMOVE: remember_opt_rect(); return 0;
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(h, &rc);
        FillRect((HDC)w, &rc, th_face_brush());
        return 1;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN: case WM_CTLCOLORLISTBOX:
        if (g_dark) return (LRESULT)th_ctlcolor(m, (HDC)w, (HWND)l, 0);
        break;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mi = (MINMAXINFO *)l;
        mi->ptMinTrackSize.x = S(660);
        mi->ptMinTrackSize.y = S(480);
        return 0;
    }
    case WM_CLOSE:   /* the X of the settings window works like Cancel: the changes are dropped, nothing is saved */
        snap_restore();
        apply_to_ui();
        hide_opt(1);
        return 0;
    }
    return DefDlgProcW(h, m, w, l);
}

static void update_opt_menu(void) {   /* the Settings Layout box of the General tab shows the layout; Options > Settings… needs the separate window */
    if (g_optsMenu) EnableMenuItem(g_optsMenu, ID_M_OPTWIN, MF_BYCOMMAND | (g_optWin ? MF_ENABLED : MF_GRAYED));
    if (W.chClassic && ctl_checked(W.chClassic) == (g_optWin != 0)) ctl_set_checked(W.chClassic, !g_optWin);
}

/* the layout of the options: tabs in the main window (classic) or in a window of their own. The controls move, their state stays. */
/* the settings window is shown in the middle of the screen (when it was not open) */
/* the settings window of its own is modal: while it is open the main window cannot be clicked */
static void sync_main_enable(void) {
    int open;
    if (g_modalDepth > 0) return;   /* a dialog has switched the main window off: not to be undone by the timer */
    open = g_optWin && g_opt && IsWindowVisible(g_opt) && !IsIconic(g_opt);
    if ((IsWindowEnabled(g_main) != 0) == !open) return;
    EnableWindow(g_main, !open);
}

/* hide the settings window: the main window is enabled first, so Windows hands the activation to it (and not to another program) */
static void hide_opt(int activateMain) {
    if (!g_opt) return;
    remember_opt_rect();
    EnableWindow(g_main, TRUE);
    ShowWindow(g_opt, SW_HIDE);
    if (activateMain && !IsIconic(g_main)) SetForegroundWindow(g_main);
}

HWND ui_owner(HWND o) { return (o == g_main && g_optWin && g_opt && IsWindowVisible(g_opt)) ? g_opt : o; }   /* dialogs opened from the settings window belong to it */

static void show_opt(void) {
    if (!IsWindowVisible(g_opt)) {
        RECT wa, wr;
        int w, h;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        GetWindowRect(g_opt, &wr);
        w = wr.right - wr.left; h = wr.bottom - wr.top;
        if (!IsIconic(g_opt)) SetWindowPos(g_opt, NULL, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ShowWindow(g_opt, IsIconic(g_opt) ? SW_RESTORE : SW_SHOW);
    sync_main_enable();
    SetForegroundWindow(g_opt);
}

static void set_optmode(int on, int show) {
    HWND par;
    int i;
    on = on != 0;
    if (on && !g_opt) {
        RECT wa;
        int w = S(690), h = S(800), x, y;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        if (h > wa.bottom - wa.top) h = wa.bottom - wa.top;
        if (w > wa.right - wa.left) w = wa.right - wa.left;
        x = wa.left + (wa.right - wa.left - w) / 2; y = wa.top + (wa.bottom - wa.top - h) / 2;
        if (g_optRect.w > 0 && on_screen(g_optRect.x, g_optRect.y, g_optRect.w, g_optRect.h)) { x = g_optRect.x; y = g_optRect.y; w = g_optRect.w; h = g_optRect.h; }
        g_opt = CreateWindowExW(0, L"ZOptions", L"Settings", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h, g_main, NULL, g_inst, NULL);
        if (!g_opt) return;
        th_window(g_opt);
    }
    g_optWin = on;
    g_set.optWindow = on;
    par = on ? g_opt : g_main;
    SetParent(W.tab, par);
    for (i = 0; i < NPAGES; i++) SetParent(W.page[i], par);   /* the pages after the tab: they are on top of it */
    SetParent(W.saveBtn, par);
    SetParent(W.cancelBtn, par);
    SetParent(W.defBtn, par);
    if (on) { layout_opt(); if (show) show_opt(); }
    else if (g_opt) hide_opt(0);
    do_layout();
    select_tab();
    RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    if (on) RedrawWindow(g_opt, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    update_opt_menu();
}

static void create_pages(void) {
    static const wchar_t *offon[] = {L"Off", L"On"};
    static const wchar_t *prescale[] = {L"1x (Off)", L"2x", L"3x", L"4x", L"5x", L"6x", L"7x", L"8x"};
    static const wchar_t *fskip[] = {L"Off", L"Auto", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"10"};
    static const wchar_t *rates[] = {L"Default (48000 Hz)", L"22050 Hz", L"32000 Hz", L"44100 Hz", L"48000 Hz"};
    static const wchar_t *latency[] = {L"1 (Lowest Delay)", L"2 (Default)", L"3", L"4", L"5 (Safest)"};
    HWND p;
    wchar_t **resl;
    int i;
    (void)offon;
    for (i = 0; i < NPAGES; i++) {
        W.page[i] = CreateWindowExW(0, L"ZPage", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | (i == 0 ? WS_VISIBLE : 0), 0, 0, 10, 10, g_main, NULL, g_inst, NULL);
        BringWindowToTop(W.page[i]);
        th_control(W.page[i], L"SCROLLBAR");   /* the dark scroll bar of a long page */
    }
    /* Video */
    p = W.page[PG_VIDEO];
    W.cbMode = add_combo(p, W.vrows, &W.nv, L"Video Mode", ID_V_MODE, kVideoModes, 5);
    W.chFull = add_check(p, W.vrows, &W.nv, L"Fullscreen", ID_V_FULL);
    W.chMax = add_check(p, W.vrows, &W.nv, L"Maximize Window", ID_V_MAX);
    resl = (wchar_t **)malloc((g_nmodes + 2) * sizeof(wchar_t *));
    resl[0] = wdup(L"Auto (MAME's Choice)");
    for (i = 0; i < g_nmodes; i++) {
        wchar_t t[40];
        swprintf(t, 40, L"%d x %d", g_modes[i].w, g_modes[i].h);
        resl[i + 1] = wdup(t);
    }
    W.cbRes = add_combo(p, W.vrows, &W.nv, L"Resolution", ID_V_RES, (const wchar_t **)resl, g_nmodes + 1);
    W.cbRot = add_combo(p, W.vrows, &W.nv, L"Rotation", ID_V_ROT, kRotations, 5);
    W.chAspect = add_check(p, W.vrows, &W.nv, L"Keep Aspect Ratio", ID_V_ASPECT);
    W.chUneven = add_check(p, W.vrows, &W.nv, L"Uneven Stretch", ID_V_UNEVEN);
    {
        static const wchar_t *ints[] = {L"Off", L"1x", L"2x", L"3x", L"4x", L"5x", L"6x", L"7x", L"8x"};
        W.cbIntScale = add_combo(p, W.vrows, &W.nv, L"Integer Scale (Pixel Perfect)", ID_V_INTSCALE, ints, 9);
    }
    W.chFilter = add_check(p, W.vrows, &W.nv, L"Bilinear Filter", ID_V_FILTER);
    W.cbPrescale = add_combo(p, W.vrows, &W.nv, L"Prescale", ID_V_PRESCALE, prescale, 8);
    W.chVsync = add_check(p, W.vrows, &W.nv, L"Wait For V-Sync", ID_V_VSYNC);
    W.chSyncRef = add_check(p, W.vrows, &W.nv, L"Sync Refresh Rate", ID_V_SYNCREF);
    W.chThrottle = add_check(p, W.vrows, &W.nv, L"Throttle (Real Speed)", ID_V_THROTTLE);
    W.cbFskip = add_combo(p, W.vrows, &W.nv, L"Frame Skip", ID_V_FSKIP, fskip, 12);
    W.chHlsl = add_check(p, W.vrows, &W.nv, L"HLSL (Direct3D Only)", ID_V_HLSL);
    W.chGlsl = add_check(p, W.vrows, &W.nv, L"GLSL (OpenGL Only)", ID_V_GLSL);
    W.bHlslSet = mk(p, L"BUTTON", L"HLSL Settings…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_V_HLSLSET);
    TIP(W.bHlslSet, L"HLSL Settings…");
    W.vrows[W.nv].lbl = NULL; W.vrows[W.nv].ctl = W.bHlslSet; W.vrows[W.nv].kind = 3; W.nv++;
    {
        static const wchar_t *glf[] = {L"Plain", L"Bilinear", L"Bicubic"};
        W.cbGlslF = add_combo(p, W.vrows, &W.nv, L"GLSL Filter", ID_V_GLSLF, glf, 3);
    }
    W.eGlslS = add_edit(p, W.vrows, &W.nv, L"GLSL Shader Name", ID_V_GLSLS, 0);
    add_header(p, W.vrows, &W.nv, L"BGFX");
    W.cbChain = add_combo(p, W.vrows, &W.nv, L"BGFX Screen Effect", ID_V_CHAIN, kChains, 6);
    W.cbBackend = add_combo(p, W.vrows, &W.nv, L"BGFX Backend", ID_V_BACKEND, kBackends, 6);
    add_header(p, W.vrows, &W.nv, L"Artwork");
    W.chCrop = add_check(p, W.vrows, &W.nv, L"Crop Artwork", ID_V_CROP);
    add_header(p, W.vrows, &W.nv, L"Display");
    mo_edit_row(p, W.vrows, &W.nv, L"Brightness", "brightness");
    mo_edit_row(p, W.vrows, &W.nv, L"Contrast", "contrast");
    mo_edit_row(p, W.vrows, &W.nv, L"Gamma", "gamma");
    mo_edit_row(p, W.vrows, &W.nv, L"Aspect Ratio", "aspect");
    add_header(p, W.vrows, &W.nv, L"BGFX Extras");
    mo_edit_row(p, W.vrows, &W.nv, L"BGFX Shadow Mask", "bgfx_shadow_mask");
    mo_edit_row(p, W.vrows, &W.nv, L"BGFX LUT", "bgfx_lut");
    /* the Filter Window group, last */
    add_header(p, W.vrows, &W.nv, L"Filter Window (Experimental)");
    {
        static const wchar_t *scal[] = {L"Smooth (Bilinear)", L"Sharp (Pixels)", L"xBRZ", L"Sharp Bilinear", L"Smooth 3D (Bicubic)"};
        static const wchar_t *dith[] = {L"Off", L"Low", L"Medium", L"High"};
        W.chFx = add_check(p, W.vrows, &W.nv, L"Filter Window", ID_V_FX);
        W.cbFxScaler = add_combo(p, W.vrows, &W.nv, L"Filter Scaler", ID_V_FXSCALER, scal, 5);
        W.chFxFxaa = add_check(p, W.vrows, &W.nv, L"FXAA Filter", ID_V_FXFXAA);
        W.cbFxDither = add_combo(p, W.vrows, &W.nv, L"Dither Smoothing", ID_V_FXDITHER, dith, 4);
    }
    for (i = 0; i <= g_nmodes; i++) free(resl[i]);
    free(resl);
    /* Audio */
    p = W.page[PG_AUDIO];
    W.cbSound = add_combo(p, W.arows, &W.na, L"Sound", ID_A_MODE, kSoundModes, 5);
    W.cbRate = add_combo(p, W.arows, &W.na, L"Sample Rate", ID_A_RATE, rates, 5);
    W.eVol = add_edit(p, W.arows, &W.na, L"Volume Attenuation (dB)", ID_A_VOL, 1);
    W.cbLat = add_combo(p, W.arows, &W.na, L"Audio Latency", ID_A_LAT, latency, 5);
    /* Controls */
    p = W.page[PG_CONTROLS];
    W.chJoy = add_check(p, W.crows, &W.nc, L"Enable Joysticks", ID_C_JOY);
    W.cbProv = add_combo(p, W.crows, &W.nc, L"Controller Provider", ID_C_PROV, kProviders, 5);
    W.eDead = add_edit(p, W.crows, &W.nc, L"Joystick Dead Zone %", ID_C_DEAD, 1);
    W.eSat = add_edit(p, W.crows, &W.nc, L"Joystick Saturation %", ID_C_SAT, 1);
    W.crows[W.nc - 1].kind = 4;   /* on the row of the dead zone */
    W.chContr = add_check(p, W.crows, &W.nc, L"Allow Contradictory Directions", ID_C_CONTR);
    W.chMouse = add_check(p, W.crows, &W.nc, L"Enable Mouse", ID_C_MOUSE);
    W.chGun = add_check(p, W.crows, &W.nc, L"Enable Light Gun", ID_C_GUN);
    {
        static const wchar_t *const gdev[] = {L"Keyboard", L"Mouse", L"Light Gun", L"Joystick", L"None"};
        static const char *const gdevv[] = {"keyboard", "mouse", "lightgun", "joystick", "none"};
        static const wchar_t *const gprov[] = {L"Auto", L"Win32", L"Raw Input"};
        static const char *const gprovv[] = {"auto", "win32", "rawinput"};
        mo_combo_row(p, W.crows, &W.nc, L"Device", "lightgun_device", gdev, gdevv, 5);
        mo_combo_row(p, W.crows, &W.nc, L"Provider", "lightgunprovider", gprov, gprovv, 3);
        W.crows[W.nc - 2].kind = 11; W.crows[W.nc - 1].kind = 11;   /* on the row of Enable Light Gun */
    }
    W.cbCtrlr = add_combo(p, W.crows, &W.nc, L"Controller File", ID_C_CTRLR, NULL, 0);
    W.chOvGame = add_check(p, W.crows, &W.nc, L"Mapping Overrides MAME's Game Settings", ID_C_OVGAME);
    TIP(W.chOvGame, L"Mapping Overrides MAME's Game Settings");
    {
        static const wchar_t *const afr[] = {L"10 Per Second", L"15 Per Second", L"20 Per Second", L"30 Per Second"};
        W.cbAf = add_combo(p, W.crows, &W.nc, L"Autofire Speed", ID_C_AFRATE, afr, 4);
    }
    add_header(p, W.crows, &W.nc, L"Key And Button Mapping");
    W.cbProf = add_combo(p, W.crows, &W.nc, L"Profile", ID_C_PROF, NULL, 0);
    TIP(W.cbProf, L"Profile");
    W.bPSave = mk(p, L"BUTTON", L"Save", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_PSAVE);
    W.bPSaveAs = mk(p, L"BUTTON", L"Save As…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_PSAVEAS);
    W.bPRen = mk(p, L"BUTTON", L"Rename…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_PREN);
    W.bPDel = mk(p, L"BUTTON", L"Delete", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_PDEL);
    TIP(W.bPSave, L"Save Profile"); TIP(W.bPSaveAs, L"Save As…"); TIP(W.bPRen, L"Rename Profile"); TIP(W.bPDel, L"Delete Profile");
    W.crows[W.nc].lbl = W.bPSave; W.crows[W.nc].ctl = W.bPSaveAs; W.crows[W.nc].btn = W.bPRen; W.crows[W.nc].clr = W.bPDel; W.crows[W.nc].kind = 8; W.nc++;
    add_note(p, W.crows, &W.nc, L"Double-click a key or pad cell, or choose a row and press Set Key / Set Pad. Autofire (buttons 1-6): click the Autofire cell, or right-click the row; the button then repeats while you hold its key / pad button (needs MAME's Lua plugins). Every action starts with the launcher's defaults (keys as listed, D-pad and left stick for directions, buttons X Y RB A B RT, Start / Back), so key and pad work together; setting a D-pad or a stick for a direction keeps the other one. Defaults restores them. Saved with OK in MAME's cfg\\default.cfg.");
    {
        static const wchar_t *bcols[] = {L"Action", L"Keyboard", L"Pad / Joystick", L"Autofire"};
        static const int bw[] = {110, 80, 150, 64};
        int i;
        W.lvBind = lv_create(p, ID_C_LV, bcols, bw, 4);
        for (i = 0; i < NBIND; i++) {
            LVITEMW it;
            memset(&it, 0, sizeof it);
            it.mask = LVIF_TEXT | LVIF_PARAM;
            it.iItem = i;
            it.pszText = (LPWSTR)bind_label(i);
            it.lParam = i;
            ListView_InsertItem(W.lvBind, &it);
        }
        W.crows[W.nc].lbl = NULL; W.crows[W.nc].ctl = W.lvBind; W.crows[W.nc].kind = 7; W.nc++;
        W.bSetKey = mk(p, L"BUTTON", L"Set Key…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_SETKEY);
        W.bSetPad = mk(p, L"BUTTON", L"Set Pad…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_SETPAD);
        W.bBindClear = mk(p, L"BUTTON", L"Clear", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_CLEAR);
        W.bBindDef = mk(p, L"BUTTON", L"Defaults", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_BDEF);
        TIP(W.bSetKey, L"Set Key…"); TIP(W.bSetPad, L"Set Pad…"); TIP(W.bBindClear, L"Clear Binding"); TIP(W.bBindDef, L"Reset Mapping");
        W.crows[W.nc].lbl = W.bSetKey; W.crows[W.nc].ctl = W.bSetPad; W.crows[W.nc].btn = W.bBindClear; W.crows[W.nc].clr = W.bBindDef; W.crows[W.nc].kind = 8; W.nc++;
    }
    W.bOpenCfg = mk(p, L"BUTTON", L"Open MAME's Cfg Folder", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_C_OPENCFG);
    TIP(W.bOpenCfg, L"Browse…");
    W.crows[W.nc].lbl = NULL; W.crows[W.nc].ctl = W.bOpenCfg; W.crows[W.nc].kind = 3; W.nc++;
    /* Combos */
    cmb_create(W.page[PG_COMBOS], W.tip);
    /* General */
    p = W.page[PG_SYSTEM];
    W.eMame = add_edit(p, W.srows, &W.ns, L"MAME Program", ID_S_MAME, 0);
    W.bMameBrowse = mk(p, L"BUTTON", L"Browse…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_S_MAMEBROWSE);
    TIP(W.bMameBrowse, L"Browse…");
    W.bMameClear = mk(p, L"BUTTON", L"X", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_S_MAMECLEAR);
    TIP(W.bMameClear, L"Clear MAME Program");
    W.srows[0].btn = W.bMameBrowse; W.srows[0].clr = W.bMameClear;
    W.eRoms = add_edit(p, W.srows, &W.ns, L"ROMs Folder", ID_S_ROMS, 0);
    W.bRomBrowse = mk(p, L"BUTTON", L"Browse…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_S_BROWSE);
    TIP(W.bRomBrowse, L"Browse…");
    W.bRomClear = mk(p, L"BUTTON", L"X", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_S_ROMCLEAR);
    TIP(W.bRomClear, L"Clear ROMs Folder");
    W.srows[1].btn = W.bRomBrowse; W.srows[1].clr = W.bRomClear;
    W.eBios = add_edit(p, W.srows, &W.ns, L"BIOS Folder", ID_S_BIOS, 0);
    W.bBiosBrowse = mk(p, L"BUTTON", L"Browse…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_S_BIOSBROWSE);
    TIP(W.bBiosBrowse, L"Browse…");
    W.bBiosClear = mk(p, L"BUTTON", L"X", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_S_BIOSCLEAR);
    TIP(W.bBiosClear, L"Clear BIOS Folder");
    W.srows[2].btn = W.bBiosBrowse; W.srows[2].clr = W.bBiosClear;
    W.chAvail = add_check(p, W.srows, &W.ns, L"List Only Available ROMs", ID_S_AVAIL);
    {
        static const wchar_t *themes[] = {L"System", L"Light", L"Dark"};
        W.cbTheme = add_combo(p, W.srows, &W.ns, L"Theme", ID_S_THEME, themes, 3);
    }
    {
        static const wchar_t *onstart[] = {L"Keep The Launcher Open", L"Minimize The Launcher", L"Close The Launcher"};
        W.cbOnStart = add_combo(p, W.srows, &W.ns, L"When A Game Starts", ID_S_ONSTART, onstart, 3);
    }
    W.chClassic = add_check(p, W.srows, &W.ns, L"Classic Layout", ID_S_LAYOUT);
    W.chHide = add_check(p, W.srows, &W.ns, L"Hide Console", ID_S_HIDE);
    W.chLogs = add_check(p, W.srows, &W.ns, L"Enable Logs", ID_S_LOGS);
    add_header(p, W.srows, &W.ns, L"Advanced");
    W.chSkip = add_check(p, W.srows, &W.ns, L"Skip Game Info Screen", ID_X_SKIP);
    W.chSkipW = add_check(p, W.srows, &W.ns, L"Skip Game Warning Screen", ID_X_SKIPW);
    W.chSave = add_check(p, W.srows, &W.ns, L"Auto Save / Restore State", ID_X_SAVE);
    W.chCheat = add_check(p, W.srows, &W.ns, L"Cheats", ID_X_CHEAT);
    W.chQuit = add_check(p, W.srows, &W.ns, L"Confirm Quit", ID_X_QUIT);
    W.eExtra = add_edit(p, W.srows, &W.ns, L"Extra Arguments", ID_X_EXTRA, 0);
    add_header(p, W.srows, &W.ns, L"Capture And Speed");
    mo_edit_row(p, W.srows, &W.ns, L"Speed", "speed");
    mo_check_row(p, W.srows, &W.ns, L"Rewind", "rewind");
    mo_edit_row(p, W.srows, &W.ns, L"Snapshot Size", "snapsize");
    mo_edit_row(p, W.srows, &W.ns, L"Record Video To", "aviwrite");
    mo_edit_row(p, W.srows, &W.ns, L"Record Audio To", "wavwrite");
    mo_edit_row(p, W.srows, &W.ns, L"Record Input To", "record");
    mo_edit_row(p, W.srows, &W.ns, L"Play Input From", "playback");
    mo_edit_row(p, W.srows, &W.ns, L"Load State", "state");
    W.bMoreOpts = mk(p, L"BUTTON", L"All MAME Options…", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_X_MOREOPTS);
    TIP(W.bMoreOpts, L"All MAME Options…");
    W.srows[W.ns].lbl = NULL; W.srows[W.ns].ctl = W.bMoreOpts; W.srows[W.ns].kind = 3; W.ns++;
    add_note(p, W.srows, &W.ns, L"Extra arguments are added to the command line of every start, after the settings of the launcher (see mame -showusage). The settings of a single game (Game > Game Settings) can add more.\nWith Enable Logs (General tab) the command line of the last start is written to mame-ex-launch.log.");
}

static void create_ui(void) {
    static const wchar_t *tabs[] = {L"General", L"Video", L"Audio", L"Controls", L"Combos"};
    static const wchar_t *cols[] = {L"Favorite", L"#", L"Game", L"Info", L"Status", L"Hardware", L"Year"};
    static const int widths[] = {64, 40, 260, 140, 40, 170, 40};
    TCITEMW ti;
    int i;
    W.tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, 0, 0, 0, 0, g_main, NULL, g_inst, NULL);
    SendMessageW(W.tip, TTM_SETMAXTIPWIDTH, 0, S(420));
    SendMessageW(W.tip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
    W.status = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_main, (HMENU)1, g_inst, NULL);
    th_status(W.status);
    SendMessageW(W.status, WM_SETFONT, (WPARAM)g_font, TRUE);
    W.lblSearch = mk(g_main, L"STATIC", L"Search:", SS_LEFT, 0, 0);
    W.search = mk(g_main, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_SEARCH);
    W.sortBtn = mk(g_main, L"BUTTON", L"Reset Filters", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_RESETSORT);
    W.clearBtn = mk(g_main, L"BUTTON", L"Close", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_CLEARSEARCH);
    W.cbRegion = mk(g_main, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_F_REGION);
    W.cbMaker = mk(g_main, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_F_MAKER);
    W.cbGenre = mk(g_main, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_F_GENRE);
    W.cbHw = mk(g_main, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_F_HW);
    SendMessageW(W.cbHw, CB_ADDSTRING, 0, (LPARAM)L"All Hardware"); SendMessageW(W.cbHw, CB_SETCURSEL, 0, 0);
    SendMessageW(W.cbRegion, CB_ADDSTRING, 0, (LPARAM)L"All Regions"); SendMessageW(W.cbRegion, CB_SETCURSEL, 0, 0);
    SendMessageW(W.cbMaker, CB_ADDSTRING, 0, (LPARAM)L"All Makers"); SendMessageW(W.cbMaker, CB_SETCURSEL, 0, 0);
    SendMessageW(W.cbGenre, CB_ADDSTRING, 0, (LPARAM)L"All Genres"); SendMessageW(W.cbGenre, CB_SETCURSEL, 0, 0); SendMessageW(W.cbHw, CB_SETCURSEL, 0, 0);
    set_tip(W.tip, g_main, W.sortBtn, tip_for(L"Reset Filters"));
    set_tip(W.tip, g_main, W.defBtn, tip_for(L"Restore Defaults"));
    set_tip(W.tip, g_main, W.cbRegion, tip_for(L"Region Filter"));
    set_tip(W.tip, g_main, W.cbMaker, tip_for(L"Maker Filter"));
    set_tip(W.tip, g_main, W.cbGenre, tip_for(L"Genre Filter"));
    set_tip(W.tip, g_main, W.cbHw, tip_for(L"Hardware Filter"));
    W.lv = lv_create(g_main, ID_GAMELIST, cols, widths, NCOLS);
    SetWindowSubclass(ListView_GetHeader(W.lv), colhdr_proc, 77, 0);
    SetWindowSubclass(W.lv, lvhdr_proc, 78, 0);
    ListView_SetExtendedListViewStyle(W.lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES | LVS_EX_SUBITEMIMAGES);
    ListView_SetColumnOrderArray(W.lv, NCOLS, (int *)kColOrder);
    ListView_SetImageList(W.lv, tb_status_images(S(16)), LVSIL_SMALL);
    ListView_SetImageList(W.lv, tb_star_images(S(16), th_gray(), RGB(255, 190, 0)), LVSIL_STATE);   /* the check box of a favorite is a star */
    /* the state holders of Favorites Only and Create Desktop Icon stay (hidden); the toolbar has the visible buttons */
    W.favOnly = mk(g_main, L"BUTTON", L"Favorites Only", BS_AUTOCHECKBOX | WS_TABSTOP, 0, ID_FAVONLY);
    W.iconBtn = mk(g_main, L"BUTTON", L"Create Desktop Icon", BS_PUSHBUTTON | WS_TABSTOP | WS_DISABLED, 0, ID_ICONBTN);
    ShowWindow(W.favOnly, SW_HIDE); ShowWindow(W.iconBtn, SW_HIDE);
    W.tbRefresh = tb_button(g_main, ID_M_RESCAN, TI_REFRESH);
    W.tbFav = tb_button(g_main, ID_TBFAV, TI_FAV);
    W.tbAvail = tb_button(g_main, ID_TBAVAIL, TI_AVAIL);
    W.tbSep1 = tb_separator(g_main);
    W.playBtn = tb_button(g_main, ID_PLAYBTN, TI_PLAY);
    W.stopBtn = tb_button(g_main, ID_STOPBTN, TI_STOP);
    EnableWindow(W.stopBtn, FALSE);
    W.tbFs = tb_button(g_main, ID_TBFS, TI_FULLSCREEN);
    W.tbSep2 = tb_separator(g_main);
    W.tbSet = tb_button(g_main, ID_TBSET, TI_SETTINGS);
    W.tbVideo = tb_button(g_main, ID_TBVIDEO, TI_VIDEO);
    W.tbAudio = tb_button(g_main, ID_TBAUDIO, TI_AUDIO);
    W.tbCtl = tb_button(g_main, ID_TBCTL, TI_CONTROLS);
    W.tbAdv = tb_button(g_main, ID_TBCMB, TI_COMBOS);
    set_tip(W.tip, g_main, W.tbRefresh, L"Refresh (F5)");
    set_tip(W.tip, g_main, W.tbFav, L"Favorites Only");
    set_tip(W.tip, g_main, W.tbAvail, L"List Only Available ROMs");
    set_tip(W.tip, g_main, W.playBtn, L"Play");
    set_tip(W.tip, g_main, W.stopBtn, L"Stop");
    set_tip(W.tip, g_main, W.tbFs, L"Fullscreen (on/off for the games you start)");
    set_tip(W.tip, g_main, W.tbSet, L"Settings");
    set_tip(W.tip, g_main, W.tbVideo, L"Video Settings");
    set_tip(W.tip, g_main, W.tbAudio, L"Audio Settings");
    set_tip(W.tip, g_main, W.tbCtl, L"Controls Settings");
    set_tip(W.tip, g_main, W.tbAdv, L"Combos Settings");
    W.tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS, 0, 0, 10, 10, g_main, (HMENU)ID_TAB, g_inst, NULL);
    SendMessageW(W.tab, WM_SETFONT, (WPARAM)g_font, TRUE);
    th_tab(W.tab);
    memset(&ti, 0, sizeof ti);
    for (i = 0; i < NPAGES; i++) { ti.mask = TCIF_TEXT; ti.pszText = (LPWSTR)tabs[i]; TabCtrl_InsertItem(W.tab, i, &ti); }
    W.saveBtn = mk(g_main, L"BUTTON", L"OK", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_SAVEBTN);
    W.cancelBtn = mk(g_main, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_CANCELBTN);
    W.defBtn = mk(g_main, L"BUTTON", L"Restore Defaults", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_DEFAULTBTN);
    create_pages();
}

static HMENU build_menu(void) {
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), game = CreatePopupMenu(), set = CreatePopupMenu(), help = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_M_RESCAN, L"&Rescan Games\tF5");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, ID_M_OPENMAME, L"Open &MAME Folder");
    AppendMenuW(file, MF_STRING, ID_M_OPENROMS, L"Open &ROMs Folder");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    g_recentMenu = CreatePopupMenu();
    AppendMenuW(file, MF_POPUP, (UINT_PTR)g_recentMenu, L"Recent &Files");
    AppendMenuW(file, MF_STRING, ID_M_CLEARRECENT, L"&Clear Recent Files");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, ID_M_OPENSNAP, L"Open &Screenshots Folder");
    AppendMenuW(file, MF_STRING, ID_M_OPENCFG, L"Open MAME &Input Mapping Folder (Cfg)");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, ID_M_BACKUP, L"&Backup Settings…");
    AppendMenuW(file, MF_STRING, ID_M_RESTORE, L"Res&tore Settings…");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, ID_M_EXIT, L"E&xit");
    AppendMenuW(game, MF_STRING, ID_M_PLAY, L"&Play\tEnter");
    AppendMenuW(game, MF_STRING, ID_M_STOP, L"&Stop");
    AppendMenuW(game, MF_STRING, ID_M_PLAYFS, L"Play &Fullscreen");
    AppendMenuW(game, MF_STRING, ID_M_PLAYWIN, L"Play &Windowed");
    AppendMenuW(game, MF_SEPARATOR, 0, NULL);
    AppendMenuW(game, MF_STRING, ID_M_GAMECFG, L"Game S&ettings…");
    AppendMenuW(game, MF_STRING, ID_M_CHECKROMS, L"Check &ROM Set…");
    AppendMenuW(game, MF_STRING, ID_M_TOGGLEFAV, L"Toggle &Favorite\tCtrl+D");
    AppendMenuW(game, MF_STRING, ID_ICONBTN, L"Create Desktop &Icon");
    AppendMenuW(set, MF_STRING, ID_M_OPTWIN, L"Se&ttings…\tF9");   /* the settings window: when the tabs are in it */
    AppendMenuW(set, MF_SEPARATOR, 0, NULL);
    AppendMenuW(set, MF_STRING, ID_TBVIDEO, L"&Video Settings");
    AppendMenuW(set, MF_STRING, ID_M_SETAUDIO, L"&Audio Settings");
    AppendMenuW(set, MF_STRING, ID_TBCTL, L"&Controls Settings");
    AppendMenuW(set, MF_STRING, ID_TBCMB, L"Co&mbos Settings");
    AppendMenuW(help, MF_STRING, ID_M_KEYS, L"&Keyboard Shortcuts");
    AppendMenuW(help, MF_STRING, ID_M_MAMEHELP, L"&MAME Documentation (Web)");
    AppendMenuW(help, MF_SEPARATOR, 0, NULL);
    AppendMenuW(help, MF_STRING, ID_M_ABOUT, L"&About");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"&File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)game, L"&Game");
    g_optsMenu = set;
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)set, L"&Options");
    {   /* View: theme, what the game list shows */
        HMENU view = CreatePopupMenu(), theme = CreatePopupMenu();
        AppendMenuW(theme, MF_STRING, ID_M_THEME0, L"&System");
        AppendMenuW(theme, MF_STRING, ID_M_THEME1, L"&Light");
        AppendMenuW(theme, MF_STRING, ID_M_THEME2, L"&Dark");
        AppendMenuW(view, MF_POPUP, (UINT_PTR)theme, L"&Theme");
        AppendMenuW(view, MF_SEPARATOR, 0, NULL);
        AppendMenuW(view, MF_STRING, ID_M_FAVONLY, L"&Favorites Only");
        AppendMenuW(view, MF_STRING, ID_M_AVAIL, L"List Only &Available ROMs");
        AppendMenuW(view, MF_SEPARATOR, 0, NULL);
        {   /* the columns of the game list */
            HMENU cols = CreatePopupMenu();
            static const wchar_t *const cn[NCOLS] = {L"&Favorite", L"&#", L"&Game", L"&Info", L"&Status", L"&Hardware", L"&Year"};
            int k;
            for (k = 0; k < NCOLS; k++) AppendMenuW(cols, MF_STRING | (kColOrder[k] == 2 ? MF_GRAYED : 0), ID_COL0 + kColOrder[k], cn[kColOrder[k]]);
            AppendMenuW(view, MF_POPUP, (UINT_PTR)cols, L"List &Columns");
            g_colsMenu = cols;
        }
        AppendMenuW(view, MF_SEPARATOR, 0, NULL);
        AppendMenuW(view, MF_STRING, ID_M_SHOWTOOLBAR, L"Show Tool&bar");
        AppendMenuW(view, MF_STRING, ID_M_SHOWSEARCH, L"Show &Search Bar");
        AppendMenuW(view, MF_STRING, ID_M_SHOWFILTERS, L"Show Filter Ba&r");
        AppendMenuW(view, MF_SEPARATOR, 0, NULL);
        AppendMenuW(view, MF_STRING, ID_M_LAY0, L"Classic &Layout");   /* checked: the settings are in the main window; unchecked: in a window of their own */
        AppendMenuW(view, MF_STRING, ID_M_RESETWIN, L"&Reset Window Size");
        g_viewMenu = view; g_themeMenu = theme;
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)view, L"&View");
    }
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, L"&Help");
    return bar;
}

/* ---------------------------------------------------------------- window procedure */
static void select_tab(void) {
    int i, s = TabCtrl_GetCurSel(W.tab);
    for (i = 0; i < NPAGES; i++) ShowWindow(W.page[i], i == s ? SW_SHOW : SW_HIDE);
}

/* the toolbar: a tab of the settings (the settings window comes to the front when the tabs are in it) */
static void show_settings(int page) {
    TabCtrl_SetCurSel(W.tab, page);
    select_tab();
    if (g_optWin && g_opt) show_opt();
}

/* a new theme (View > Theme, or the Theme box of the General tab with OK): saved, applied at the next start */
static void apply_theme_mode(int mode) {
    if (mode == g_set.themeMode) return;
    g_set.themeMode = mode;
    set_combo(W.cbTheme, mode);
    settings_resolve_theme();
    settings_save();
    if (g_set.dark != g_dark) {
        int r = 0;
        msg_box(g_main, L"Theme", L"The theme changes after a restart.\n\nRestart the launcher now?", MB_YESNO | MB_ICONQUESTION, &r);
        if (r == IDYES) {
            wchar_t self[MAX_PATH];
            if (do_save() && GetModuleFileNameW(NULL, self, MAX_PATH)) {
                if ((INT_PTR)ShellExecuteW(g_main, NULL, self, NULL, NULL, SW_SHOWNORMAL) > 32) SendMessageW(g_main, WM_CLOSE, 0, 0);
                else set_note(L"Could not restart: start the launcher again yourself");
            }
        } else set_note(L"The theme changes at the next start");
    } else set_note(L"Theme saved");
}

static void on_command(int id, int code, HWND src) {
    if (id >= ID_RECENT0 && id < ID_RECENT0 + 8) { launch_recent(id - ID_RECENT0); return; }
    if (id >= ID_K_STEP && id < ID_K_TOK + CMB_NTOK) { cmb_command(id, code); return; }
    switch (id) {
    case ID_M_RESCAN: rescan(1); return;
    case ID_M_OPENMAME: { wchar_t d[600]; mame_dir(d, 600); open_folder(d); return; }
    case ID_M_OPENCFG: case ID_C_OPENCFG: { wchar_t d[600], m[560]; mame_dir(m, 560); path_join(d, 600, m, L"cfg"); CreateDirectoryW(d, NULL); open_folder(d); return; }
    case ID_M_MAMEHELP: ShellExecuteW(g_main, L"open", L"https://docs.mamedev.org/", NULL, NULL, SW_SHOWNORMAL); return;
    case ID_M_OPENROMS: { wchar_t d[600]; roms_dir_abs(d, 600); CreateDirectoryW(d, NULL); open_folder(d); return; }
    case ID_M_CLEARRECENT: il_clear(&g_set.recent); settings_save(); rebuild_recent(); return;
    case ID_M_EXIT: SendMessageW(g_main, WM_CLOSE, 0, 0); return;
    case ID_M_PLAY: case ID_PLAYBTN: if (code == BN_CLICKED || code == 1 || id == ID_M_PLAY) launch_game(selected_game()); return;
    case ID_M_STOP: case ID_STOPBTN: if (g_running) { g_stopped = 1; mame_stop(&g_proc); } return;
    case ID_M_TOGGLEFAV: toggle_fav(); return;
    case ID_M_PLAYFS: launch_once(selected_game(), 1); return;
    case ID_M_PLAYWIN: launch_once(selected_game(), 0); return;
    case ID_M_GAMECFG: game_settings(); return;
    case ID_M_OPENSNAP: { wchar_t d[600], m[560]; mame_dir(m, 560); path_join(d, 600, m, L"snap"); CreateDirectoryW(d, NULL); open_folder(d); return; }
    case ID_M_BACKUP: do_backup(); return;
    case ID_M_RESTORE: do_restore(); return;
    case ID_S_MAMEBROWSE: browse_mame(); return;
    case ID_S_MAMECLEAR: SetWindowTextW(W.eMame, L""); ctrlr_fill(g_set.ctrlr); return;
    case ID_S_ROMCLEAR: {
        int r = 0;
        wchar_t cur[560];
        GetWindowTextW(W.eRoms, cur, 560);
        if (!cur[0]) return;
        msg_box(g_main, L"Clear ROMs Folder", L"Clear the ROMs folder?\n\nThe roms folder of MAME is used until you choose a folder again.", MB_YESNO | MB_ICONQUESTION, &r);
        if (r != IDYES) return;
        SetWindowTextW(W.eRoms, L"");
        g_set.roms[0] = 0;
        refilter();
        return;
    }
    case ID_M_CHECKROMS: { const Game *g = selected_game(); if (g) dlg_check_roms(g_main, g); else msg_box(g_main, L"Check ROM Set", L"Select a game first.", MB_ICONINFORMATION, NULL); return; }
    case ID_M_THEME0: case ID_M_THEME1: case ID_M_THEME2: apply_theme_mode(id - ID_M_THEME0); return;   /* View > Theme */
    case ID_M_FAVONLY: SendMessageW(W.favOnly, BM_CLICK, 0, 0); return;
    case ID_M_SHOWTOOLBAR:
        g_set.hideToolbar = !g_set.hideToolbar; apply_bars(); do_layout(); settings_save();
        RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);   /* the Save Settings button moved */
        return;
    case ID_M_SHOWSEARCH: g_set.hideSearch = SEARCH_SHOWN(); g_searchOv = -1; apply_bars(); do_layout(); settings_save(); return;   /* shown -> hidden, hidden -> shown (and remembered) */
    case ID_M_SHOWFILTERS: g_set.hideFilters = !g_set.hideFilters; apply_bars(); do_layout(); settings_save(); return;
    case ID_COL2: return;   /* the Game column always stays */
    case ID_COL0: case ID_COL1: case ID_COL3: case ID_COL4: case ID_COL5: case ID_COL6:   /* show / hide a column of the game list */
        g_set.colsHidden ^= 1 << (id - ID_COL0);
        apply_columns();
        settings_save();
        return;
    case ID_M_OPTWIN: case ID_OPTBTN:   /* F9 / Options > Settings… / the Settings button: show the settings window (only when the options are in it) */
        if (g_optWin) { set_optmode(1, 0); show_settings(PG_SYSTEM); }   /* always the General tab */
        return;
    case ID_SAVEBTN:   /* OK: save; a settings window of its own closes */
        if (do_save()) set_note(L"Settings saved");
        if (g_optWin && g_opt) hide_opt(1);
        { int mode = (int)SendMessageW(W.cbTheme, CB_GETCURSEL, 0, 0); if (mode >= 0 && mode != g_set.themeMode) apply_theme_mode(mode); }   /* the Theme box */
        return;
    case ID_CANCELBTN:   /* Cancel: the changes are dropped (the saved settings are shown again); a settings window of its own closes */
        snap_restore();
        apply_to_ui();
        if (g_optWin && g_opt) hide_opt(1);
        return;
    case ID_M_LAY0: {   /* Classic Layout on / off, from the View menu */
        int want = g_optWin ? 0 : 1;   /* in a window of their own now: back to classic; else: to a window of their own */
        ctl_set_checked(W.chClassic, !want);
        g_set.optWindow = want;
        settings_save();
        PostMessageW(g_main, WM_APP_LAYOUT, (WPARAM)want, 0);
        return;
    }
    case ID_M_RESETWIN: reset_window(0); set_note(L"Window size reset"); return;
    case ID_M_FOCUSSEARCH: if (!SEARCH_SHOWN()) { g_searchOv = 1; apply_bars(); do_layout(); }   /* only for now: the bar stays hidden at the next start */
         SetFocus(W.search); SendMessageW(W.search, EM_SETSEL, 0, -1); return;
    case ID_DEFAULTBTN: default_settings(); return;
    case ID_M_KEYS: {
        static const TableRow KEYS[] = {
            {L"LAUNCHER", NULL},
            {L"Enter / Ctrl+Enter", L"Play the selected game"}, {L"Double click", L"Play the game"},
            {L"Right click", L"Menu of the game (play options, favorite, desktop icon)"},
            {L"Ctrl+D", L"Toggle favorite"}, {L"Ctrl+F", L"Search"}, {L"F5", L"Rescan games"}, {L"F9", L"Settings"},
            {L"", L""},
            {L"", L""},
            {L"PLAYER 1", L"(the launcher's defaults)"},
            {L"W  /  S  /  A  /  D", L"Up / down / left / right (pad: D-pad and left stick)"},
            {L"Num 4", L"Button 1 (pad: X)"}, {L"Num 5", L"Button 2 (pad: Y)"}, {L"Num 6", L"Button 3 (pad: RB)"},
            {L"Num 1", L"Button 4 (pad: A)"}, {L"Num 2", L"Button 5 (pad: B)"}, {L"Num 3", L"Button 6 (pad: RT)"},
            {L"Enter", L"Start (pad: Start)"}, {L"Right Shift", L"Insert coin (pad: Back)"},
            {L"", L""},
            {L"PLAYER 2", NULL},
            {L"Arrow keys", L"Up / down / left / right"},
            {L"U", L"Button 1"}, {L"I", L"Button 2"}, {L"O", L"Button 3"},
            {L"J", L"Button 4"}, {L"K", L"Button 5"}, {L"L", L"Button 6"},
            {L"Y", L"Start"}, {L"H", L"Insert coin"},
            {L"", L""},
            {L"MAME'S OWN KEYS", NULL},
            {L"F2", L"Test switch (the service menu of the board)"}, {L"F3", L"Soft reset"}, {L"Shift+F3", L"Hard reset"},
            {L"Tab", L"MAME menu: input mapping, DIP switches, video options"},
            {L"P", L"Pause"}, {L"F10", L"Throttle on / off"}, {L"F11", L"Show frames per second"},
            {L"F12", L"Screenshot"}, {L"Alt+Enter", L"Fullscreen / window"},
            {L"F7 / Shift+F7", L"Save / load a state"}, {L"Esc", L"Quit the game"},
            {L"", L""},
            {L"NOTE", NULL}, {L"", L"The game keys can be changed in the Controls tab (Set Key / Set Pad, right click, Autofire for the buttons) or in MAME: Tab > Input (this Machine)."}};
        dlg_table(g_main, L"Keyboard Shortcuts", L"Key", L"Action", KEYS, (int)(sizeof KEYS / sizeof KEYS[0]), 140, 400);
        return;
    }
    case ID_M_ABOUT: dlg_about(g_main); return;
    case ID_SEARCH: if (code == EN_CHANGE && !g_applying && g_games) refilter(); return;
    case ID_CLEARSEARCH:   /* Close: the search is cleared and the search bar hidden for this session (Ctrl+F shows it again) */
        SetWindowTextW(W.search, L"");
        g_searchOv = 0;   /* hidden for now: the saved View > Search Bar choice is not touched */
        apply_bars(); do_layout();
        SetFocus(W.lv);
        return;
    case ID_FAVONLY: if (code == BN_CLICKED) { g_set.favOnly = ctl_checked(W.favOnly); tb_set_checked(W.tbFav, g_set.favOnly); refilter(); } return;
    case ID_TBAVAIL: case ID_M_AVAIL:   /* List Only Available ROMs: toggled directly (a click message does not reach the check box when its window is hidden) */
        ctl_set_checked(W.chAvail, !ctl_checked(W.chAvail));
        g_set.onlyAvail = ctl_checked(W.chAvail);
        refilter();
        settings_save();
        return;
    case ID_TBAUDIO: show_settings(PG_AUDIO); return;
    case ID_TBFS: {   /* the toolbar switch: the Fullscreen setting (the check box of the Video tab) on / off, saved at once */
        ctl_set_checked(W.chFull, !ctl_checked(W.chFull));
        tb_set_checked(W.tbFs, ctl_checked(W.chFull));
        g_set.fullscreen = ctl_checked(W.chFull);
        settings_save();
        snap_take();   /* saved at once: Cancel must not undo it */
        update_state();
        set_note(ctl_checked(W.chFull) ? L"Fullscreen is on" : L"Fullscreen is off (window)");
        return;
    }
    case ID_V_FULL: if (code == BN_CLICKED) { tb_set_checked(W.tbFs, ctl_checked(W.chFull)); update_state(); } return;
    case ID_V_MODE: case ID_A_MODE: if (code == CBN_SELCHANGE) update_state(); return;
    case ID_V_GLSL: case ID_V_FX: case ID_V_HLSL: if (code == BN_CLICKED) update_state(); return;
    case ID_V_HLSLSET: hlsl_dialog(ui_owner(g_main)); return;
    case ID_X_MOREOPTS: more_options_dialog(ui_owner(g_main)); return;
    case ID_C_JOY: if (code == BN_CLICKED) update_state(); return;
    case ID_TBFAV: SendMessageW(W.favOnly, BM_CLICK, 0, 0); return;   /* the toolbar switch clicks the hidden check box: one place for the logic */
    case ID_TBSET: show_settings(PG_SYSTEM); return;
    case ID_TBVIDEO: show_settings(PG_VIDEO); return;
    case ID_M_SETAUDIO: show_settings(PG_AUDIO); return;
    case ID_TBCTL: show_settings(PG_CONTROLS); return;
    case ID_TBCMB: show_settings(PG_COMBOS); return;
    case ID_ICONBTN: desktop_icon(); return;
    case ID_C_PROF: case ID_C_PSAVE: case ID_C_PSAVEAS: case ID_C_PREN: case ID_C_PDEL: prof_command(id, code); return;
    case ID_C_SETKEY: case ID_C_SETPAD: {
        int row = bind_selected();
        if (row < 0) { set_note(L"Choose an action in the list first"); return; }
        if (id == ID_C_SETKEY ? bind_capture_key(ui_owner(g_main), row) : bind_capture_pad(ui_owner(g_main), row)) bind_fill();
        return;
    }
    case ID_C_CLEAR: {
        int row = bind_selected();
        if (row < 0) { set_note(L"Choose an action in the list first"); return; }
        bind_clear(row, 1); bind_clear(row, 2);
        bind_fill();
        return;
    }
    case ID_C_BDEF: {
        int r = 0;
        msg_box(g_main, L"Reset Mapping", L"Give all the actions of the list back to the default keys and pad buttons?\n\nThe changes are written when you press OK.", MB_YESNO | MB_ICONQUESTION, &r);
        if (r == IDYES) { bind_reset_all(); bind_fill(); }
        return;
    }
    case ID_RESETSORT:   /* all filters back to "All" and the list back to its normal order */
        SendMessageW(W.cbRegion, CB_SETCURSEL, 0, 0); SendMessageW(W.cbMaker, CB_SETCURSEL, 0, 0); SendMessageW(W.cbGenre, CB_SETCURSEL, 0, 0); SendMessageW(W.cbHw, CB_SETCURSEL, 0, 0);
        g_sortCol = 1; g_sortAsc = 1; refilter(); settings_save(); return;
    case ID_F_REGION: case ID_F_MAKER: case ID_F_GENRE: case ID_F_HW: if (code == CBN_SELCHANGE) { refilter(); settings_save(); } return;
    case ID_S_ROMS: case ID_S_BIOS: if (code == EN_KILLFOCUS) refilter(); return;
    case ID_S_BIOSBROWSE: browse_bios(); return;
    case ID_S_BIOSCLEAR: SetWindowTextW(W.eBios, L""); refilter(); return;
    case ID_S_BROWSE: browse_roms(); return;
    case ID_S_AVAIL: if (code == BN_CLICKED) refilter(); return;
    }
    (void)src;
}

static LRESULT on_notify(NMHDR *nh) {
    if (nh->idFrom == ID_TAB && nh->code == TCN_SELCHANGE) { select_tab(); return 0; }
    if (nh->code == NM_CUSTOMDRAW && (nh->idFrom == ID_GAMELIST || nh->idFrom == ID_C_LV || nh->idFrom == ID_K_LV))
        return lv_altrows((LPNMLVCUSTOMDRAW)nh);
    if (nh->idFrom == ID_K_LV) return cmb_notify(nh) ? 0 : 0;
    if (nh->idFrom == ID_C_LV && nh->code == NM_RCLICK) {   /* a right click on a row: the menu Set / Clear for the key or the pad (or both, on the action) */
        LVHITTESTINFO ht;
        POINT pt, sp;
        HMENU m;
        int pick;
        memset(&ht, 0, sizeof ht);
        GetCursorPos(&sp);
        pt = sp;
        ScreenToClient(W.lvBind, &pt);
        ht.pt = pt;
        ListView_SubItemHitTest(W.lvBind, &ht);
        if (ht.iItem < 0) return 0;
        ListView_SetItemState(W.lvBind, ht.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m = CreatePopupMenu();
        if (bind_is_button(ht.iItem)) { AppendMenuW(m, MF_STRING | (bind_get_autofire(ht.iItem) ? MF_CHECKED : 0), 5, L"Autofire"); AppendMenuW(m, MF_SEPARATOR, 0, NULL); }
        if (ht.iSubItem != 2) { AppendMenuW(m, MF_STRING, 1, L"Set Key…"); }
        if (ht.iSubItem != 1) { AppendMenuW(m, MF_STRING, 2, L"Set Pad…"); }
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
        if (ht.iSubItem != 2) AppendMenuW(m, MF_STRING, 3, L"Clear Key");
        if (ht.iSubItem != 1) AppendMenuW(m, MF_STRING, 4, L"Clear Pad");
        pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, sp.x, sp.y, 0, g_main, NULL);
        DestroyMenu(m);
        if (pick == 1 && bind_capture_key(ui_owner(g_main), ht.iItem)) bind_fill();
        else if (pick == 2 && bind_capture_pad(ui_owner(g_main), ht.iItem)) bind_fill();
        else if (pick == 3) { bind_clear(ht.iItem, 1); bind_fill(); }
        else if (pick == 4) { bind_clear(ht.iItem, 2); bind_fill(); }
        else if (pick == 5) { bind_set_autofire(ht.iItem, !bind_get_autofire(ht.iItem)); bind_fill(); }
        return 0;
    }
    if (nh->idFrom == ID_C_LV && nh->code == NM_CLICK) {   /* the Autofire column works like a check box column */
        int row, col;
        if (lv_hit_cell(W.lvBind, (LPARAM)nh, &row, &col) && col == 3 && bind_is_button(row)) { bind_set_autofire(row, !bind_get_autofire(row)); bind_fill(); }
        return 0;
    }
    if (nh->idFrom == ID_C_LV && nh->code == NM_DBLCLK) {   /* a double click on the key or the pad cell of an action: asks for it */
        LVHITTESTINFO ht;
        POINT pt;
        memset(&ht, 0, sizeof ht);
        GetCursorPos(&pt);
        ScreenToClient(W.lvBind, &pt);
        ht.pt = pt;
        ListView_SubItemHitTest(W.lvBind, &ht);
        if (ht.iItem >= 0 && ht.iSubItem >= 1) {
            ListView_SetItemState(W.lvBind, ht.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            if (ht.iSubItem == 3) { if (bind_is_button(ht.iItem)) { bind_set_autofire(ht.iItem, !bind_get_autofire(ht.iItem)); bind_fill(); } }
            else if (ht.iSubItem == 1 ? bind_capture_key(ui_owner(g_main), ht.iItem) : bind_capture_pad(ui_owner(g_main), ht.iItem)) bind_fill();
        }
        return 0;
    }
    if (nh->idFrom == ID_GAMELIST) {
        switch (nh->code) {
        case LVN_ITEMCHANGED: {
            NMLISTVIEW *n = (NMLISTVIEW *)nh;
            if (g_rebuilding || !(n->uChanged & LVIF_STATE)) return 0;
            if ((n->uNewState ^ n->uOldState) & LVIS_STATEIMAGEMASK) {
                int on = ((n->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2;
                if (n->iItem >= 0 && n->iItem < g_nview) {
                    set_favorite(g_games[g_view[n->iItem]].id, on);
                    if (ctl_checked(W.favOnly)) PostMessageW(g_main, WM_APP_REFILTER, 0, 0);   /* the list is rebuilt once this click is done */
                }
            }
            if ((n->uNewState ^ n->uOldState) & LVIS_SELECTED) { update_buttons(); selection_status(); }
            return 0;
        }
        case LVN_COLUMNCLICK: {
            int c = ((NMLISTVIEW *)nh)->iSubItem;
            if (c == 3) return 0;   /* the Info column is not sorted */
            if (c == g_sortCol) g_sortAsc = !g_sortAsc; else { g_sortCol = c; g_sortAsc = 1; }
            refilter();
            settings_save();   /* the sort order is remembered at once, whatever way the program is left */
            return 0;
        }
        case NM_DBLCLK: {   /* double-click on a game plays it (not on its check box) */
            LVHITTESTINFO ht;
            POINT pt;
            memset(&ht, 0, sizeof ht);
            GetCursorPos(&pt);
            ScreenToClient(W.lv, &pt);
            ht.pt = pt;
            ListView_HitTest(W.lv, &ht);
            if (ht.iItem >= 0 && !((ht.flags & LVHT_ONITEMSTATEICON) && !(ht.flags & LVHT_ONITEMLABEL))) launch_game(selected_game());
            return 0;
        }
        case NM_RCLICK: {   /* right-click menu of the game under the mouse */
            LVHITTESTINFO ht;
            LPNMITEMACTIVATE ia = (LPNMITEMACTIVATE)nh;
            const Game *g;
            HMENU pm;
            POINT pt;
            memset(&ht, 0, sizeof ht);
            ht.pt = ia->ptAction;
            ListView_HitTest(W.lv, &ht);
            if (ht.iItem < 0) return 0;
            ListView_SetItemState(W.lv, ht.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            g = selected_game();
            if (!g) return 0;
            pm = CreatePopupMenu();
            AppendMenuW(pm, MF_STRING | (playable(g) ? 0 : MF_GRAYED), ID_M_PLAY, L"Play");
            AppendMenuW(pm, MF_STRING | (playable(g) ? 0 : MF_GRAYED), ID_M_PLAYFS, L"Play Fullscreen");
            AppendMenuW(pm, MF_STRING | (playable(g) ? 0 : MF_GRAYED), ID_M_PLAYWIN, L"Play Windowed");
            AppendMenuW(pm, MF_SEPARATOR, 0, NULL);
            AppendMenuW(pm, MF_STRING, ID_M_GAMECFG, gamecfg_find(g->id) ? L"Game Settings… (Own Settings Set)" : L"Game Settings…");
            AppendMenuW(pm, MF_STRING, ID_M_CHECKROMS, L"Check ROM Set…");
            AppendMenuW(pm, MF_STRING, ID_ICONBTN, L"Create Desktop Icon");
            AppendMenuW(pm, MF_STRING, ID_M_TOGGLEFAV, il_has(&g_set.fav, g->id) ? L"Remove From Favorite" : L"Add To Favorite");
            GetCursorPos(&pt);
            TrackPopupMenu(pm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, NULL);
            DestroyMenu(pm);
            return 0;
        }
        case LVN_KEYDOWN:
            if (((NMLVKEYDOWN *)nh)->wVKey == VK_RETURN) launch_game(selected_game());
            return 0;
        }
        return 0;
    }
    return 0;
}

static void remember_opt_rect(void);

static void remember_window(void) {
    WINDOWPLACEMENT wp;
    remember_opt_rect();
    lv_get_widths(W.lv, g_set.colsGame, NCOLS);
    { int k; for (k = 0; k < NCOLS; k++) if (((g_set.colsHidden >> k) & 1) && g_colW[k] > 0) g_set.colsGame[k] = MulDiv(g_colW[k], 96, g_dpi); }   /* a hidden column keeps the width it will come back with */
    g_set.splitX = MulDiv(g_splitX, 96, g_dpi);
    wp.length = sizeof wp;
    if (GetWindowPlacement(g_main, &wp) && wp.showCmd == SW_SHOWNORMAL) {
        RECT r;
        GetWindowRect(g_main, &r);
        if (r.right - r.left > 200 && r.bottom - r.top > 200) { g_winRect.x = r.left; g_winRect.y = r.top; g_winRect.w = r.right - r.left; g_winRect.h = r.bottom - r.top; }
    }
}

static LRESULT CALLBACK main_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        g_main = h;
        create_ui();
        return 0;
    case WM_SIZE:
        if (W.status && w != SIZE_MINIMIZED) {
            do_layout();
            /* after maximizing, buttons next to the moved controls could stay unpainted (white) or leave stray lines: repaint everything */
            RedrawWindow(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            SetTimer(h, 3, 150, NULL);   /* and once more when the maximize / restore animation is over */
        }
        return 0;
    case WM_DRAWITEM: if (tb_draw((const DRAWITEMSTRUCT *)l)) return TRUE; break;   /* the toolbar buttons */
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN: case WM_CTLCOLORLISTBOX:   /* the controls on the window itself (search row, filters) */
        if (g_dark) return (LRESULT)th_ctlcolor(m, (HDC)w, (HWND)l, 0);
        break;
    case 0x91: case 0x92:   /* the menu bar of the dark theme */
        if (g_dark) { int hnd; LRESULT r = th_menu_msg(h, m, w, l, &hnd); if (hnd) return r; }
        break;
    case WM_NCPAINT: case WM_NCACTIVATE:
        if (g_dark) { LRESULT r = DefWindowProcW(h, m, w, l); th_menu_line(h); return r; }
        break;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mi = (MINMAXINFO *)l;
        { int minw = g_set.optWindow ? S(640) : S(1000);   /* without the tabs the main window can be narrower */
          mi->ptMinTrackSize.x = minw > GetSystemMetrics(SM_CXSCREEN) ? GetSystemMetrics(SM_CXSCREEN) : minw; }
        mi->ptMinTrackSize.y = S(700) > GetSystemMetrics(SM_CYSCREEN) ? GetSystemMetrics(SM_CYSCREEN) : S(700);
        return 0;
    }
    case WM_COMMAND: on_command(LOWORD(w), HIWORD(w), (HWND)l); return 0;
    case WM_INITMENUPOPUP: {   /* Play in the Game menu follows the selected game */
        HMENU mn = (HMENU)w;
        if (mn == g_viewMenu) CheckMenuItem(g_viewMenu, ID_M_LAY0, MF_BYCOMMAND | (g_optWin ? MF_UNCHECKED : MF_CHECKED));
        if (mn == g_colsMenu) { int k; for (k = 0; k < NCOLS; k++) CheckMenuItem(g_colsMenu, ID_COL0 + k, MF_BYCOMMAND | (((g_set.colsHidden >> k) & 1) ? MF_UNCHECKED : MF_CHECKED)); }
        if (mn == g_viewMenu || mn == g_themeMenu) {   /* the check marks of the View menu */
            CheckMenuRadioItem(g_themeMenu, ID_M_THEME0, ID_M_THEME2, ID_M_THEME0 + g_set.themeMode, MF_BYCOMMAND);
            CheckMenuItem(g_viewMenu, ID_M_FAVONLY, MF_BYCOMMAND | (ctl_checked(W.favOnly) ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(g_viewMenu, ID_M_AVAIL, MF_BYCOMMAND | (ctl_checked(W.chAvail) ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(g_viewMenu, ID_M_SHOWSEARCH, MF_BYCOMMAND | (SEARCH_SHOWN() ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(g_viewMenu, ID_M_SHOWTOOLBAR, MF_BYCOMMAND | (g_set.hideToolbar ? MF_UNCHECKED : MF_CHECKED));
            CheckMenuItem(g_viewMenu, ID_M_SHOWFILTERS, MF_BYCOMMAND | (g_set.hideFilters ? MF_UNCHECKED : MF_CHECKED));
        }
        if (GetMenuState(mn, ID_M_PLAY, MF_BYCOMMAND) != (UINT)-1) {
            int ok = playable(selected_game()) ? MF_ENABLED : MF_GRAYED;
            EnableMenuItem(mn, ID_M_PLAYFS, MF_BYCOMMAND | ok);
            EnableMenuItem(mn, ID_M_PLAYWIN, MF_BYCOMMAND | ok);
            EnableMenuItem(mn, ID_M_GAMECFG, MF_BYCOMMAND | (selected_game() ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(mn, ID_M_CHECKROMS, MF_BYCOMMAND | (selected_game() ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(mn, ID_ICONBTN, MF_BYCOMMAND | (selected_game() ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(mn, ID_M_PLAY, MF_BYCOMMAND | ok);
            EnableMenuItem(mn, ID_M_STOP, MF_BYCOMMAND | (g_running ? MF_ENABLED : MF_GRAYED));
            EnableMenuItem(mn, ID_M_TOGGLEFAV, MF_BYCOMMAND | (selected_game() ? MF_ENABLED : MF_GRAYED));
        }
        return 0;
    }
    case WM_ACTIVATE:   /* ROMs may have been added while the window was in the background */
        if (LOWORD(w) != WA_INACTIVE && W.eRoms && g_games) { if (roms_refresh(0)) PostMessageW(g_main, WM_APP_REFILTER, 0, 0); update_buttons(); }
        return 0;
    case WM_NOTIFY: return on_notify((NMHDR *)l);
    case WM_APP_LAYOUT: set_optmode((int)w == 1, 0); reset_window(1); return 0;   /* a new layout: the window sizes go back to the defaults of it */
    case WM_APP_REFILTER: refilter(); return 0;
    case WM_APP_CELL: cmb_cell(LOWORD(l), HIWORD(l)); return 0;
    case WM_APP_REFILTER + 100: {   /* MAME said the ROMs are missing / wrong: its own check, with the files it wants */
        const Game *lg = NULL;
        int k;
        for (k = 0; k < g_ngames; k++) if (g_games[k].id == g_lastGameId) lg = &g_games[k];
        if (lg) dlg_check_roms(g_main, lg);
        return 0;
    }
    case WM_APP_GAMES: {
        FetchResult *r = (FetchResult *)l;
        wchar_t exe[560];
        games_free(g_games, g_ngames);
        g_games = r->list; g_ngames = r->n;
        games_sort_default(g_games, g_ngames);
        roms_refresh(1);
        games_status();
        free(r);
        fill_filters();
        refilter();
        rebuild_recent();
        if (!mame_exe(exe, 560)) {   /* the list is there, but nothing can be started */
            wchar_t t[400];
            swprintf(t, 400, L"mame.exe was not found (General tab > MAME Program)   |   %d games listed", g_ngames);
            set_status(t);
            if (!g_mameWarned) {
                g_mameWarned = 1;
                msg_box(g_main, L"MAME Not Found", L"mame.exe was not found next to the launcher.\n\nChoose it on the General tab (MAME Program), or copy this launcher into the MAME folder.", MB_ICONINFORMATION, NULL);
                show_settings(PG_SYSTEM);
            }
        }
        return 0;
    }
    case WM_APP_GAMEEXIT:
        filter_stop();
        {
            DWORD code = 0, secs = (GetTickCount() - g_startTick) / 1000;
            int hint = 0;
            if (g_running) {
                GetExitCodeProcess(g_proc.hProcess, &code);
                hint = !g_stopped && (code != 0 || secs < 5);   /* a game that ends at once or with an error has a problem to find */
                CloseHandle(g_proc.hProcess); CloseHandle(g_proc.hThread); g_running = 0;
            }
            if (g_minimized) {   /* the launcher comes back */
                g_minimized = 0;
                if (IsIconic(g_main)) ShowWindow(g_main, SW_RESTORE);
                SetForegroundWindow(g_main);
            }
            EnableWindow(W.stopBtn, FALSE);
            update_buttons();
            games_status();
            if (hint) {
                wchar_t t[300];
                static const wchar_t *why[] = {L"", L"the game failed MAME's validity check", L"ROM files are missing or wrong (Game > Check ROM Set)", L"MAME had a fatal error",
                                               L"a device could not be loaded", L"MAME does not know this set", L"an option the launcher passed is not valid for this MAME (see Enable Logs on the General tab)",
                                               L"none of the ROM files were recognised", L"MAME failed to start the game"};
                if (code >= 1 && code <= 8) swprintf(t, 300, L"MAME exited with code %lu: %ls", (unsigned long)code, why[code]);
                else swprintf(t, 300, L"MAME closed after %lu s (exit code %lu): use Game > Check ROM Set, and turn on Enable Logs to see the command line", (unsigned long)secs, (unsigned long)code);
                set_note(t);
                if (code == 1 || code == 2 || code == 7) PostMessageW(g_main, WM_APP_REFILTER + 100, 0, 0);   /* the ROM check shows what MAME is missing */
                else {   /* what MAME printed before it ended: a video module that cannot start, an unknown option ... */
                    wchar_t *tail = mame_output_tail(1400);
                    if (tail[0] && code != 0) {
                        wchar_t *msg = (wchar_t *)malloc((wcslen(tail) + 200) * sizeof(wchar_t));
                        swprintf(msg, wcslen(tail) + 200, L"%ls\n\nMAME said:\n\n%ls", t, tail);
                        msg_box(g_main, L"MAME Ended", msg, MB_ICONWARNING, NULL);
                        free(msg);
                    }
                    free(tail);
                }
            }
        }
        return 0;
    case WM_TIMER:
        if (w == 1) {
            static int tick;
            sync_main_enable();   /* the main window is off while the settings window is open */
            if (++tick % 8 == 0 && g_games && !g_applying && roms_refresh(0)) PostMessageW(g_main, WM_APP_REFILTER, 0, 0);   /* now and then: ROMs may be added or removed while the launcher is open */
            update_buttons();
        }
        else if (w == 3) {
            KillTimer(h, 3);
            RedrawWindow(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        else if (w == 2) {
            wchar_t cur[200] = L"";
            KillTimer(h, 2);
            get_status(cur, 200);
            if (!wcscmp(cur, g_note)) set_status(L"");
        }
        return 0;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(l), lx = S(8) + g_splitX;
        if (!g_optWin && x >= lx && x < lx + S(8)) { g_dragging = 1; SetCapture(h); }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_dragging) { g_splitX = (short)LOWORD(l) - S(8) - S(4); do_layout(); RedrawWindow(h, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW); }
        return 0;
    case WM_LBUTTONUP:
        if (g_dragging) { g_dragging = 0; ReleaseCapture(); }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(h, &pt);
            if (!g_optWin && pt.x >= S(8) + g_splitX && pt.x < S(16) + g_splitX) { SetCursor(LoadCursorW(NULL, IDC_SIZEWE)); return TRUE; }
        }
        break;
    case WM_CLOSE:
        filter_stop();
        snap_restore();   /* settings changed but not confirmed with OK are not saved */
        remember_window();
        settings_save();   /* the window, the columns and the choices made so far */
        DestroyWindow(h);
        return 0;
    case WM_ENDSESSION:   /* logoff / shutdown: the window, the columns and the choices made so far are kept */
        if (w) {
            filter_stop();
            snap_restore();
            remember_window();
            settings_save();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---------------------------------------------------------------- start */
static int on_screen(int x, int y, int w, int h) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return x + w > vx + 100 && x < vx + vw - 100 && y + 60 > vy && y < vy + vh - 100;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show) {
    wchar_t *slash;
    WNDCLASSEXW wc;
    HACCEL acc;
    ACCEL ac[6];
    MSG msg;
    int argc, W0, H0, x, y;
    wchar_t **argv;
    HWND hw;
    (void)prev; (void)cmd; (void)show;
    GetModuleFileNameW(NULL, g_root, 560);
    slash = wcsrchr(g_root, L'\\');
    if (slash) *slash = 0;
    {   /* the files of the builds that were called ZiNc EX - MAME Edition: moved to the new names once */
        static const wchar_t *const oldNew[][2] = {{L"zinc-settings.cfg", L"mame-ex-settings.cfg"}, {L"zinc-games.cfg", L"mame-ex-games.cfg"}, {L"zinc-combos.cfg", L"mame-ex-combos.cfg"}};
        int k;
        for (k = 0; k < 3; k++) {
            wchar_t a[560], b[560];
            path_join(a, 560, g_root, oldNew[k][0]);
            path_join(b, 560, g_root, oldNew[k][1]);
            if (file_exists(a) && !file_exists(b)) MoveFileW(a, b);
        }
    }
    {   /* one launcher per folder: a second one would overwrite the settings of the first (it brings the first to the front and ends) */
        wchar_t name[600] = L"MAME-EX-";
        size_t k, n = wcslen(name);
        for (k = 0; g_root[k] && n < 590; k++, n++) name[n] = (g_root[k] == L'\\' || g_root[k] == L':') ? L'_' : (wchar_t)towlower(g_root[k]);
        name[n] = 0;
        CreateMutexW(NULL, FALSE, name);
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND other = FindWindowW(L"MameExMain", NULL);
            if (other) { if (IsIconic(other)) ShowWindow(other, SW_RESTORE); SetForegroundWindow(other); }
            return 0;
        }
    }
    path_join(g_settingsFile, 560, g_root, L"mame-ex-settings.cfg");
    g_firstRun = !file_exists(g_settingsFile);
    settings_defaults(&g_set, NULL);
    settings_load();
    gamecfg_load();
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {   /* frontends: MAME-EX.exe --game <set or ROM file> [--fullscreen | --windowed]  (or just the ROM file) */
        const wchar_t *game = NULL;
        int fs = -1, i;
        for (i = 1; i < argc; i++) {
            if (!_wcsicmp(argv[i], L"--fullscreen")) fs = 1;
            else if (!_wcsicmp(argv[i], L"--windowed")) fs = 0;
            else if (!_wcsicmp(argv[i], L"--game") && i + 1 < argc) game = argv[++i];
            else if (!_wcsnicmp(argv[i], L"--game=", 7)) game = argv[i] + 7;
            else if (argv[i][0] != L'-' && !game) game = argv[i];
        }
        if (game) { int rc = run_game_cli(game, fs); LocalFree(argv); return rc; }
    }
    th_init(g_set.dark);
    ui_init(inst);
    load_modes();

    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = main_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = th_face_brush();
    wc.lpszClassName = L"MameExMain";
    wc.hIcon = g_icon;
    wc.hIconSm = g_iconSm;
    RegisterClassExW(&wc);
    wc.lpfnWndProc = page_proc;
    wc.cbWndExtra = DLGWINDOWEXTRA;
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"ZPage";
    wc.hIcon = wc.hIconSm = NULL;
    RegisterClassExW(&wc);
    wc.lpfnWndProc = opt_proc;   /* the options window */
    wc.hbrBackground = th_face_brush();
    wc.lpszClassName = L"ZOptions";
    wc.hIcon = g_icon;
    wc.hIconSm = g_iconSm;
    RegisterClassExW(&wc);

    W0 = g_set.optWindow ? S(860) : S(1340); H0 = g_set.optWindow ? S(720) : S(800);   /* without the tabs the main window is narrower */
    {
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        if (W0 > wa.right - wa.left) W0 = wa.right - wa.left;
        if (H0 > wa.bottom - wa.top) H0 = wa.bottom - wa.top;
        x = wa.left + (wa.right - wa.left - W0) / 2;
        y = wa.top + (wa.bottom - wa.top - H0) / 2;
    }
    if (g_winRect.w > 0 && on_screen(g_winRect.x, g_winRect.y, g_winRect.w, g_winRect.h)) { x = g_winRect.x; y = g_winRect.y; W0 = g_winRect.w; H0 = g_winRect.h; }
    hw = CreateWindowExW(0, L"MameExMain", L"MAME EX", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, W0, H0, NULL, build_menu(), inst, NULL);
    if (!hw) return 1;
    th_window(hw);
    ac[0].fVirt = FVIRTKEY; ac[0].key = VK_F5; ac[0].cmd = ID_M_RESCAN;
    ac[1].fVirt = FVIRTKEY | FCONTROL; ac[1].key = VK_RETURN; ac[1].cmd = ID_M_PLAY;
    ac[2].fVirt = FVIRTKEY | FCONTROL; ac[2].key = 'D'; ac[2].cmd = ID_M_TOGGLEFAV;
    ac[3].fVirt = FVIRTKEY | FCONTROL; ac[3].key = 'F'; ac[3].cmd = ID_M_FOCUSSEARCH;
    ac[4].fVirt = FVIRTKEY; ac[4].key = VK_F9; ac[4].cmd = ID_M_OPTWIN;
    acc = CreateAcceleratorTableW(ac, 5);
    ShowWindow(hw, SW_SHOW);
    do_layout();
    select_tab();
    apply_to_ui();
    update_opt_menu();
    apply_bars();
    do_layout();
    if (g_set.optWindow) set_optmode(1, 0);   /* the options window stays closed until it is asked for */
    rebuild_recent();
    SetTimer(hw, 1, 400, NULL);   /* widgets have many change events: simply compare with the saved state a few times a second */
    rescan(0);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        HWND dlg = NULL, p;
        if (TranslateAcceleratorW(hw, acc, &msg)) continue;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && (msg.hwnd == W.lv || msg.hwnd == W.search || msg.hwnd == W.favOnly)) {   /* Enter plays the selected game (the dialog handling would swallow it) */
            if (!(msg.lParam & (1 << 30)) && selected_game()) launch_game(selected_game());
            continue;
        }
        for (p = msg.hwnd; p; p = GetParent(p)) {
            wchar_t cls[16];
            GetClassNameW(p, cls, 16);
            if (!wcscmp(cls, L"ZPage") || !wcscmp(cls, L"MameExMain") || !wcscmp(cls, L"ZOptions")) { dlg = p; break; }
        }
        if (dlg && IsDialogMessageW(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
