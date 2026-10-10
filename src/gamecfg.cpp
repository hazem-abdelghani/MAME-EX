/* Per-game settings: what a game does differently from the main settings (video mode, window / resolution, rotation,
   sound, cheats ...). Everything left on "Same As Global" follows the main settings.
   Stored in mame-ex-games.cfg; mame_args() adds them to the command line of the game. */
#include "common.h"

static GameCfg *g_gc;
static int g_ngc;

#define NOVOL (-100)

static void gc_init(GameCfg *c, int id) {
    memset(c, 0, sizeof *c);
    c->id = id;
    c->video = c->fullscreen = c->rotate = c->keepAspect = c->filter = c->waitVsync = c->chain = c->sound = c->cheat = c->skipInfo = c->autosave = kSame;
    c->volume = NOVOL;
}

int gamecfg_active(const GameCfg *c) {
    return c && (c->video >= 0 || c->fullscreen >= 0 || c->resW > 0 || c->rotate >= 0 || c->keepAspect >= 0 || c->filter >= 0 || c->waitVsync >= 0 ||
                 c->chain >= 0 || c->sound >= 0 || c->volume != NOVOL || c->cheat >= 0 || c->skipInfo >= 0 || c->autosave >= 0 || c->bios[0] || c->extra[0]);
}

const GameCfg *gamecfg_find(int id) {
    int i;
    for (i = 0; i < g_ngc; i++) if (g_gc[i].id == id) return gamecfg_active(&g_gc[i]) ? &g_gc[i] : NULL;
    return NULL;
}

static GameCfg *gc_slot(int id) {
    int i;
    for (i = 0; i < g_ngc; i++) if (g_gc[i].id == id) return &g_gc[i];
    g_gc = (GameCfg *)realloc(g_gc, (size_t)(g_ngc + 1) * sizeof(GameCfg));
    gc_init(&g_gc[g_ngc], id);
    return &g_gc[g_ngc++];
}

void gamecfg_set(const GameCfg *c) { *gc_slot(c->id) = *c; }

void gamecfg_remove(int id) {
    int i;
    for (i = 0; i < g_ngc; i++) if (g_gc[i].id == id) { memmove(&g_gc[i], &g_gc[i + 1], (size_t)(g_ngc - i - 1) * sizeof(GameCfg)); g_ngc--; return; }
}

static void cfg_path(wchar_t *out, size_t cap) { path_join(out, cap, g_root, L"mame-ex-games.cfg"); }

static void rd_tri(const char *val, int *dst, int hi) { int x; if (xatoi(val, &x) && x >= 0 && x <= hi) *dst = x; }

void gamecfg_load(void) {
    wchar_t p[560];
    char *txt, *line, *save = NULL;
    GameCfg *cur = NULL;
    free(g_gc); g_gc = NULL; g_ngc = 0;
    cfg_path(p, 560);
    txt = read_file(p, NULL);
    if (!txt) return;
    for (line = strtok_s(txt, "\n", &save); line; line = strtok_s(NULL, "\n", &save)) {
        char *eq, *key, *val;
        line = trim_a(line);
        if (!*line || *line == ';' || *line == '#') continue;
        if (*line == '[') {
            int id;
            char *close = strchr(line, ']');
            cur = NULL;
            if (close) *close = 0;
            if (!strncmp(line, "[game ", 6) && xatoi(trim_a(line + 6), &id)) cur = gc_slot(id);
            continue;
        }
        if (!cur) continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        key = trim_a(line);
        val = trim_a(eq + 1);
        _strlwr(key);
        if (!strcmp(key, "video")) rd_tri(val, &cur->video, 4);
        else if (!strcmp(key, "fullscreen")) rd_tri(val, &cur->fullscreen, 1);
        else if (!strcmp(key, "size")) { int w, h; if (sscanf(val, "%dx%d", &w, &h) == 2 && w >= 160 && h >= 120 && w <= 7680 && h <= 4320) { cur->resW = w; cur->resH = h; } }
        else if (!strcmp(key, "rotate")) rd_tri(val, &cur->rotate, 4);
        else if (!strcmp(key, "keep_aspect")) rd_tri(val, &cur->keepAspect, 1);
        else if (!strcmp(key, "filter")) rd_tri(val, &cur->filter, 1);
        else if (!strcmp(key, "wait_vsync")) rd_tri(val, &cur->waitVsync, 1);
        else if (!strcmp(key, "bgfx_chain")) rd_tri(val, &cur->chain, 5);
        else if (!strcmp(key, "sound")) rd_tri(val, &cur->sound, 1);
        else if (!strcmp(key, "volume")) { int x; if (xatoi(val, &x) && x >= -32 && x <= 0) cur->volume = x; }
        else if (!strcmp(key, "cheat")) rd_tri(val, &cur->cheat, 1);
        else if (!strcmp(key, "skip_gameinfo")) rd_tri(val, &cur->skipInfo, 1);
        else if (!strcmp(key, "autosave")) rd_tri(val, &cur->autosave, 1);
        else if (!strcmp(key, "bios")) { wchar_t *w = u8_to_w(val, -1); wcsncpy(cur->bios, w, 39); free(w); }
        else if (!strcmp(key, "extra_arguments")) { wchar_t *w = u8_to_w(val, -1); wcsncpy(cur->extra, w, 259); free(w); }
    }
    free(txt);
}

int gamecfg_save(void) {
    wchar_t p[560];
    Buf b = {0};
    int i, n = 0, ok = 1;
    cfg_path(p, 560);
    buf_add(&b, "; MAME EX per-game settings (written by mame-ex; a missing line means: same as the main settings)\r\n");
    for (i = 0; i < g_ngc; i++) {
        const GameCfg *c = &g_gc[i];
        if (!gamecfg_active(c)) continue;
        n++;
        buf_fmt(&b, "\r\n[game %d]\r\n", c->id);
        if (c->video >= 0) buf_fmt(&b, "video=%d\r\n", c->video);
        if (c->fullscreen >= 0) buf_fmt(&b, "fullscreen=%d\r\n", c->fullscreen);
        if (c->resW > 0) buf_fmt(&b, "size=%dx%d\r\n", c->resW, c->resH);
        if (c->rotate >= 0) buf_fmt(&b, "rotate=%d\r\n", c->rotate);
        if (c->keepAspect >= 0) buf_fmt(&b, "keep_aspect=%d\r\n", c->keepAspect);
        if (c->filter >= 0) buf_fmt(&b, "filter=%d\r\n", c->filter);
        if (c->waitVsync >= 0) buf_fmt(&b, "wait_vsync=%d\r\n", c->waitVsync);
        if (c->chain >= 0) buf_fmt(&b, "bgfx_chain=%d\r\n", c->chain);
        if (c->sound >= 0) buf_fmt(&b, "sound=%d\r\n", c->sound);
        if (c->volume != NOVOL) buf_fmt(&b, "volume=%d\r\n", c->volume);
        if (c->cheat >= 0) buf_fmt(&b, "cheat=%d\r\n", c->cheat);
        if (c->skipInfo >= 0) buf_fmt(&b, "skip_gameinfo=%d\r\n", c->skipInfo);
        if (c->autosave >= 0) buf_fmt(&b, "autosave=%d\r\n", c->autosave);
        if (c->bios[0]) { char *u = w_to_u8(c->bios); buf_fmt(&b, "bios=%s\r\n", u); free(u); }
        if (c->extra[0]) { char *u = w_to_u8(c->extra); buf_fmt(&b, "extra_arguments=%s\r\n", u); free(u); }
    }
    if (n == 0) { DeleteFileW(p); buf_free(&b); return 1; }
    ok = write_file(p, b.s, b.n);
    buf_free(&b);
    return ok;
}

/* ---------------------------------------------------------------- the dialog */
typedef struct {
    HWND cb[20], lbl[20];
    int tab[20];   /* the tab of every row */
    HWND tabs, eExtra, bDips;
    const Game *game;
    const GameInfo *info;
    GameCfg cfg;
    int nmodes;
    int reset;
} Dlg;

/* the rows, by tab: General, Video, Audio, Other */
enum { R_VIDEO, R_WIN, R_RES, R_ROT,
       R_ASPECT, R_FILTER, R_VSYNC, R_CHAIN,
       R_SOUND, R_VOL,
       R_CHEAT, R_SKIP, R_SAVE, R_BIOS, NROWS };
#define TOPY 84      /* the first row, below the tabs */
#define MAXROWS 9    /* the most rows of a tab */
#define ID_G_RESET 1001
#define ID_G_DIPS 1002

static int tri(int v) { return v < 0 ? 0 : v ? 1 : 2; }          /* -1 / 1 / 0 -> combo index (Same / On / Off) */
static int untri(int i) { return i == 0 ? -1 : i == 1 ? 1 : 0; }

static int dlg_ok(Modal *md) {
    Dlg *d = (Dlg *)md->user;
    GameCfg *c = &d->cfg;
    int i;
    if (d->reset) { gc_init(c, c->id); return 1; }
    i = (int)SendMessageW(d->cb[R_VIDEO], CB_GETCURSEL, 0, 0); c->video = i <= 0 ? -1 : i - 1;
    i = (int)SendMessageW(d->cb[R_WIN], CB_GETCURSEL, 0, 0); c->fullscreen = i <= 0 ? -1 : i == 2 ? 1 : 0;
    i = (int)SendMessageW(d->cb[R_RES], CB_GETCURSEL, 0, 0);
    c->resW = c->resH = 0;
    if (i > 0 && i <= d->nmodes) ui_mode_get(i - 1, &c->resW, &c->resH);
    i = (int)SendMessageW(d->cb[R_ROT], CB_GETCURSEL, 0, 0); c->rotate = i <= 0 ? -1 : i - 1;
    c->keepAspect = untri((int)SendMessageW(d->cb[R_ASPECT], CB_GETCURSEL, 0, 0));
    c->filter = untri((int)SendMessageW(d->cb[R_FILTER], CB_GETCURSEL, 0, 0));
    c->waitVsync = untri((int)SendMessageW(d->cb[R_VSYNC], CB_GETCURSEL, 0, 0));
    i = (int)SendMessageW(d->cb[R_CHAIN], CB_GETCURSEL, 0, 0); c->chain = i <= 0 ? -1 : i - 1;
    c->sound = untri((int)SendMessageW(d->cb[R_SOUND], CB_GETCURSEL, 0, 0));
    i = (int)SendMessageW(d->cb[R_VOL], CB_GETCURSEL, 0, 0); c->volume = i <= 0 ? NOVOL : -(i - 1) * 4;   /* 0, -4 ... -32 dB */
    c->cheat = untri((int)SendMessageW(d->cb[R_CHEAT], CB_GETCURSEL, 0, 0));
    c->skipInfo = untri((int)SendMessageW(d->cb[R_SKIP], CB_GETCURSEL, 0, 0));
    c->autosave = untri((int)SendMessageW(d->cb[R_SAVE], CB_GETCURSEL, 0, 0));
    i = (int)SendMessageW(d->cb[R_BIOS], CB_GETCURSEL, 0, 0);
    if (d->info) c->bios[0] = 0;   /* without the list of BIOS (MAME did not answer) the combo is disabled: the saved one stays */
    if (i > 0 && d->info && i - 1 < (int)d->info->bios.size()) { wchar_t *w = u8_to_w(d->info->bios[i - 1].name.c_str(), -1); wcsncpy(c->bios, w, 39); free(w); }
    GetWindowTextW(d->eExtra, c->extra, 260);
    trim_w(c->extra);
    return 1;
}

static int dlg_cmd(Modal *md, int id) {
    Dlg *d = (Dlg *)md->user;
    if (id == ID_G_RESET) { d->reset = 1; SendMessageW(md->dlg, WM_COMMAND, IDOK, 0); return 1; }
    if (id == ID_G_DIPS) { dlg_dip_switches(md->dlg, d->game); return 1; }
    return 0;
}

static LRESULT dlg_notify(Modal *md, NMHDR *nh) {   /* another tab: its rows are shown, the rows of the others are hidden */
    Dlg *d = (Dlg *)md->user;
    if (nh->hwndFrom == d->tabs && nh->code == TCN_SELCHANGE) {
        int s = TabCtrl_GetCurSel(d->tabs), i;
        for (i = 0; i < NROWS; i++) { ShowWindow(d->cb[i], d->tab[i] == s ? SW_SHOW : SW_HIDE); ShowWindow(d->lbl[i], d->tab[i] == s ? SW_SHOW : SW_HIDE); }
        ShowWindow(d->eExtra, s == 3 ? SW_SHOW : SW_HIDE);
        ShowWindow((HWND)GetPropW(d->eExtra, L"lbl"), s == 3 ? SW_SHOW : SW_HIDE);
        ShowWindow(d->bDips, s == 3 ? SW_SHOW : SW_HIDE);
    }
    return 0;
}

static HWND add_row(Modal *md, Dlg *d, int row, int tab, int pos, const wchar_t *label, const wchar_t *const *items, int n, int sel) {
    int y = TOPY + pos * 28, i;
    HWND cb;
    d->lbl[row] = mkat(md->dlg, L"STATIC", label, SS_LEFT, 12, y + 4, 150, 18, 0);
    d->tab[row] = tab;
    cb = mkat(md->dlg, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 170, y, 290, 240, 0);
    for (i = 0; i < n; i++) SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageW(cb, CB_SETCURSEL, sel < 0 || sel >= n ? 0 : sel, 0);
    d->cb[row] = cb;
    return cb;
}

/* Game > Game Settings: returns 1 when the settings of the game changed */
int dlg_game_settings(HWND owner, const Game *g) {
    static const wchar_t *triItems[] = {L"Same As Global", L"On", L"Off"};
    static const wchar_t *winItems[] = {L"Same As Global", L"Window", L"Fullscreen"};
    static const wchar_t *tn[] = {L"General", L"Video", L"Audio", L"Other"};
    Modal md;
    Dlg d;
    const GameCfg *old = gamecfg_find(g->id);
    wchar_t head[600], t[40];
    const wchar_t **items;
    int i, n, changed;
    memset(&md, 0, sizeof md);
    memset(&d, 0, sizeof d);
    gc_init(&d.cfg, g->id);
    if (old) d.cfg = *old;
    md.kind = 6; md.owner = owner; md.onok = dlg_ok; md.oncmd = dlg_cmd; md.user = &d;
    wcscpy(md.title, L"Game Settings");
    make_modal(&md, 480, TOPY + MAXROWS * 28 + 56);
    swprintf(head, 600, L"%ls\n\"Same As Global\" follows the main settings.", g->title);
    mkat(md.dlg, L"STATIC", head, SS_LEFT | SS_NOPREFIX, 12, 8, 456, 34, 0);
    md.onnotify = dlg_notify;
    {
        TCITEMW ti;
        int k;
        d.tabs = mkat(md.dlg, WC_TABCONTROLW, L"", WS_TABSTOP, 12, 46, 456, 28, 0);
        th_tab(d.tabs);   /* dark in the dark theme */
        memset(&ti, 0, sizeof ti);
        for (k = 0; k < 4; k++) { ti.mask = TCIF_TEXT; ti.pszText = (LPWSTR)tn[k]; TabCtrl_InsertItem(d.tabs, k, &ti); }
    }
    /* General */
    items = (const wchar_t **)calloc((size_t)(6), sizeof(wchar_t *));
    items[0] = L"Same As Global";
    for (i = 0; i < 5; i++) items[i + 1] = kVideoModes[i];
    add_row(&md, &d, R_VIDEO, 0, 0, L"Video Mode", items, 6, d.cfg.video < 0 ? 0 : d.cfg.video + 1);
    free(items);
    add_row(&md, &d, R_WIN, 0, 1, L"Window", winItems, 3, d.cfg.fullscreen < 0 ? 0 : d.cfg.fullscreen ? 2 : 1);
    d.nmodes = ui_mode_count();
    items = (const wchar_t **)calloc((size_t)(d.nmodes + 1), sizeof(wchar_t *));
    items[0] = L"Same As Global";
    for (n = 0, i = 0; i < d.nmodes; i++) {
        int w, h;
        ui_mode_get(i, &w, &h);
        swprintf(t, 40, L"%d x %d", w, h);
        items[i + 1] = wdup(t);
        if (w == d.cfg.resW && h == d.cfg.resH) n = i + 1;
    }
    add_row(&md, &d, R_RES, 0, 2, L"Resolution", items, d.nmodes + 1, n);
    for (i = 1; i <= d.nmodes; i++) free((void *)items[i]);
    free(items);
    {
        const wchar_t *rot[6];
        rot[0] = L"Same As Global";
        for (i = 0; i < 5; i++) rot[i + 1] = kRotations[i];
        add_row(&md, &d, R_ROT, 0, 3, L"Rotation", rot, 6, d.cfg.rotate < 0 ? 0 : d.cfg.rotate + 1);
    }
    /* Video */
    add_row(&md, &d, R_ASPECT, 1, 0, L"Keep Aspect Ratio", triItems, 3, tri(d.cfg.keepAspect));
    add_row(&md, &d, R_FILTER, 1, 1, L"Bilinear Filter", triItems, 3, tri(d.cfg.filter));
    add_row(&md, &d, R_VSYNC, 1, 2, L"Wait For V-Sync", triItems, 3, tri(d.cfg.waitVsync));
    {
        const wchar_t *ch[7];
        ch[0] = L"Same As Global";
        for (i = 0; i < 6; i++) ch[i + 1] = kChains[i];
        add_row(&md, &d, R_CHAIN, 1, 3, L"BGFX Screen Effect", ch, 7, d.cfg.chain < 0 ? 0 : d.cfg.chain + 1);
    }
    /* Audio */
    add_row(&md, &d, R_SOUND, 2, 0, L"Sound", triItems, 3, tri(d.cfg.sound));
    {
        const wchar_t *vol[10];
        static wchar_t vt[9][24];
        vol[0] = L"Same As Global";
        for (i = 0; i < 9; i++) { swprintf(vt[i], 24, i == 0 ? L"Full (0 dB)" : L"Quieter (-%d dB)", i * 4); vol[i + 1] = vt[i]; }
        add_row(&md, &d, R_VOL, 2, 1, L"Volume", vol, 10, d.cfg.volume == NOVOL ? 0 : -d.cfg.volume / 4 + 1);
    }
    /* Other */
    add_row(&md, &d, R_CHEAT, 3, 0, L"Cheats", triItems, 3, tri(d.cfg.cheat));
    add_row(&md, &d, R_SKIP, 3, 1, L"Skip Game Info Screen", triItems, 3, tri(d.cfg.skipInfo));
    add_row(&md, &d, R_SAVE, 3, 2, L"Auto Save / Restore State", triItems, 3, tri(d.cfg.autosave));
    {   /* the BIOS versions of the game (asked from MAME once per game) */
        const wchar_t *bi[16];
        static wchar_t bt[15][80];
        int nb = 1, sel = 0;
        HCURSOR oc = SetCursor(LoadCursor(NULL, IDC_WAIT));
        d.game = g;
        d.info = game_info(g->set);
        SetCursor(oc);
        bi[0] = L"Game Default";
        if (d.info && d.info->ok)
            for (size_t k = 0; k < d.info->bios.size() && nb < 16; k++) {
                wchar_t *wn = u8_to_w(d.info->bios[k].name.c_str(), -1), *wd = u8_to_w(d.info->bios[k].desc.c_str(), -1);
                swprintf(bt[nb - 1], 80, L"%ls: %ls", wn, wd);
                if (!_wcsicmp(wn, d.cfg.bios)) sel = nb;
                free(wn); free(wd);
                bi[nb] = bt[nb - 1]; nb++;
            }
        add_row(&md, &d, R_BIOS, 3, 3, L"BIOS", bi, nb, sel);
        if (nb == 1) EnableWindow(d.cb[R_BIOS], FALSE);
    }
    {
        HWND l = mkat(md.dlg, L"STATIC", L"Extra Arguments", SS_LEFT, 12, TOPY + 4 * 28 + 4, 150, 18, 0);
        d.eExtra = mkat(md.dlg, L"EDIT", d.cfg.extra, ES_AUTOHSCROLL | WS_TABSTOP, 170, TOPY + 4 * 28, 290, 22, 0);
        SetWindowLongW(d.eExtra, GWL_EXSTYLE, GetWindowLongW(d.eExtra, GWL_EXSTYLE) | WS_EX_CLIENTEDGE);
        SetWindowPos(d.eExtra, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
        SendMessageW(d.eExtra, EM_SETLIMITTEXT, 259, 0);
        SetPropW(d.eExtra, L"lbl", (HANDLE)l);
        ShowWindow(d.eExtra, SW_HIDE); ShowWindow(l, SW_HIDE);
        d.bDips = mkat(md.dlg, L"BUTTON", L"DIP Switches…", BS_PUSHBUTTON | WS_TABSTOP, 170, TOPY + 5 * 28, 290, 26, ID_G_DIPS);
        ShowWindow(d.bDips, SW_HIDE);
    }
    {   /* only the rows of the first tab are shown */
        int k;
        for (k = 0; k < NROWS; k++) if (d.tab[k] != 0) { ShowWindow(d.cb[k], SW_HIDE); ShowWindow(d.lbl[k], SW_HIDE); }
    }
    mkat(md.dlg, L"BUTTON", L"Reset", BS_PUSHBUTTON | WS_TABSTOP, 12, TOPY + MAXROWS * 28 + 12, 90, 26, ID_G_RESET);
    mkat(md.dlg, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 290, TOPY + MAXROWS * 28 + 12, 80, 26, IDOK);
    mkat(md.dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 380, TOPY + MAXROWS * 28 + 12, 80, 26, IDCANCEL);
    run_modal(&md);
    changed = md.changed;
    if (changed) {
        if (gamecfg_active(&d.cfg)) gamecfg_set(&d.cfg); else gamecfg_remove(g->id);
        gamecfg_save();
    }
    return changed;
}
