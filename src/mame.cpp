/* Finding MAME, building its command line from the settings and starting it. */
#include "common.h"
#include <string>

/* ---------------------------------------------------------------- argument lists */
void args_free(ArgList *l) { int i; for (i = 0; i < l->n; i++) free(l->a[i]); free(l->a); l->a = NULL; l->n = 0; }
static void arg_add(ArgList *l, const wchar_t *s) { l->a = (wchar_t **)realloc(l->a, (l->n + 1) * sizeof(wchar_t *)); l->a[l->n++] = wdup(s); }
static void arg_addf(ArgList *l, const wchar_t *fmt, ...) {
    wchar_t tmp[1500];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(tmp, 1500, fmt, ap);
    va_end(ap);
    tmp[1499] = 0;
    arg_add(l, tmp);
}
static void arg_flag(ArgList *l, const wchar_t *name, int on) { arg_addf(l, on ? L"-%ls" : L"-no%ls", name); }   /* -window / -nowindow */

/* one argument the way the C runtime of Windows reads it back: quoted when it has blanks, quotes inside it escaped */
static std::wstring quote_arg(const wchar_t *a) {
    std::wstring out;
    size_t i, n = wcslen(a);
    if (n && !wcspbrk(a, L" \t\"")) return a;
    out += L'"';
    for (i = 0;; i++) {
        size_t slashes = 0;
        while (i < n && a[i] == L'\\') { slashes++; i++; }
        if (i == n) { out.append(slashes * 2, L'\\'); break; }
        if (a[i] == L'"') { out.append(slashes * 2 + 1, L'\\'); out += L'"'; }
        else { out.append(slashes, L'\\'); out += a[i]; }
    }
    out += L'"';
    return out;
}

wchar_t *args_join(const ArgList *a) {
    std::wstring s;
    int i;
    for (i = 0; i < a->n; i++) { if (i) s += L' '; s += quote_arg(a->a[i]); }
    return wdup(s.c_str());
}

wchar_t *command_line_join(const wchar_t *exe, const ArgList *a) {
    wchar_t *rest = args_join(a);
    std::wstring s = quote_arg(exe);
    if (rest[0]) { s += L' '; s += rest; }
    free(rest);
    return wdup(s.c_str());
}

/* ---------------------------------------------------------------- where MAME is */
static void abs_path(const wchar_t *p, wchar_t *out, size_t cap) {
    if (is_abs_path(p)) { wcsncpy(out, p, cap - 1); out[cap - 1] = 0; } else path_join(out, cap, g_root, p);
}

int mame_exe(wchar_t *out, size_t cap) {
    static const wchar_t *const cand[] = {L"mame.exe", L"mame\\mame.exe", L"mame64.exe", L"mame\\mame64.exe"};
    size_t i;
    if (g_set.mame[0]) { abs_path(g_set.mame, out, cap); return file_exists(out); }
    for (i = 0; i < sizeof cand / sizeof cand[0]; i++) { path_join(out, cap, g_root, cand[i]); if (file_exists(out)) return 1; }
    path_join(out, cap, g_root, cand[0]);
    return 0;
}

void mame_dir(wchar_t *out, size_t cap) {
    wchar_t *slash;
    mame_exe(out, cap);
    for (slash = out; *slash; slash++) if (*slash == L'/') *slash = L'\\';
    slash = wcsrchr(out, L'\\');
    if (slash && slash > out && slash[-1] == L':') slash[1] = 0;   /* D:\mame.exe -> D:\ (not the drive-relative D:) */
    else if (slash) *slash = 0; else { wcsncpy(out, g_root, cap - 1); out[cap - 1] = 0; }
}

void roms_dir_abs(wchar_t *out, size_t cap) {
    wchar_t first[560], *semi;
    wcsncpy(first, g_set.roms, 559);
    first[559] = 0;
    semi = wcschr(first, L';');
    if (semi) *semi = 0;
    trim_w(first);
    if (first[0]) { abs_path(first, out, cap); return; }
    mame_dir(first, 560);
    path_join(out, cap, first, L"roms");
}

int mame_ctrlr_names(wchar_t names[][64], int max) {
    wchar_t dir[560], pat[600];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int n = 0;
    mame_dir(dir, 560);
    path_join(pat, 600, dir, L"ctrlr\\*.cfg");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        size_t l = wcslen(fd.cFileName);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || l < 5 || n >= max) continue;
        wcsncpy(names[n], fd.cFileName, 63);
        names[n][63] = 0;
        names[n][wcslen(names[n]) - 4] = 0;   /* MAME wants the name without .cfg */
        n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

/* ---------------------------------------------------------------- the command line of a game */
/* what a game really gets: the main settings, changed by the settings of the game itself (mame-ex-games.cfg) and by the one-off options of a start */
typedef struct {
    int video, fullscreen, resW, resH, rotate, keepAspect, filter, waitVsync, chain, sound, volume, cheat, skipInfo, autosave;
} Eff;

static void effective(int id, const PlayOpts *po, Eff *e, const GameCfg **gcOut) {
    const GameCfg *gc = gamecfg_find(id);
    e->video = g_set.video; e->fullscreen = g_set.fullscreen; e->resW = g_set.resW; e->resH = g_set.resH; e->rotate = g_set.rotate;
    e->keepAspect = g_set.keepAspect; e->filter = g_set.filter; e->waitVsync = g_set.waitVsync; e->chain = g_set.chain;
    e->sound = g_set.sound; e->volume = g_set.volume; e->cheat = g_set.cheat; e->skipInfo = g_set.skipInfo; e->autosave = g_set.autosave;
    if (gc) {
        if (gc->video >= 0) e->video = gc->video;
        if (gc->fullscreen >= 0) e->fullscreen = gc->fullscreen;
        if (gc->resW > 0) { e->resW = gc->resW; e->resH = gc->resH; }
        if (gc->rotate >= 0) e->rotate = gc->rotate;
        if (gc->keepAspect >= 0) e->keepAspect = gc->keepAspect;
        if (gc->filter >= 0) e->filter = gc->filter;
        if (gc->waitVsync >= 0) e->waitVsync = gc->waitVsync;
        if (gc->chain >= 0) e->chain = gc->chain;
        if (gc->sound >= 0) e->sound = gc->sound ? (g_set.sound == 1 ? 0 : g_set.sound) : 1;   /* on: the main sound mode, or auto when that is off */
        if (gc->volume != -100) e->volume = gc->volume;
        if (gc->cheat >= 0) e->cheat = gc->cheat;
        if (gc->skipInfo >= 0) e->skipInfo = gc->skipInfo;
        if (gc->autosave >= 0) e->autosave = gc->autosave;
    }
    if (po && po->fullscreen >= 0) e->fullscreen = po->fullscreen;
    *gcOut = gc;
}

/* "a b "c d"" -> separate arguments */
static void add_words(ArgList *a, const wchar_t *text) {
    int n = 0, i;
    wchar_t **w;
    if (!text[0]) return;
    w = CommandLineToArgvW((std::wstring(L"x ") + text).c_str(), &n);
    if (!w) return;
    for (i = 1; i < n; i++) arg_add(a, w[i]);
    LocalFree(w);
}

static int g_afPlugin;   /* the combos / autofire plugin is part of the command line of the last mame_args */
void mame_args(int id, const PlayOpts *po, ArgList *a) {
    Eff e;
    const GameCfg *gc;
    const Game *g = NULL;
    int i;
    memset(a, 0, sizeof *a);
    for (i = 0; i < g_ngames; i++) if (g_games[i].id == id) { g = &g_games[i]; break; }
    effective(id, po, &e, &gc);
    arg_add(a, g ? g->set : L"");
    if (gc && gc->bios[0]) { arg_add(a, L"-bios"); arg_add(a, gc->bios); }   /* the BIOS chosen for this game */
    if (g_set.roms[0] || g_set.biosDir[0]) {   /* the ROMs folders (and the BIOS folder) as full paths: the launcher's folder is not the working directory of MAME */
        std::wstring rp;
        wchar_t *copy = wdup(g_set.roms), *tok, *save = NULL, full[600];
        for (tok = wcstok_s(copy, L";", &save); tok; tok = wcstok_s(NULL, L";", &save)) {
            trim_w(tok);
            if (!tok[0]) continue;
            abs_path(tok, full, 600);
            if (!rp.empty()) rp += L';';
            rp += full;
        }
        free(copy);
        if (g_set.biosDir[0]) {   /* the BIOS sets are looked for there too (the roms folder stays first: without a ROMs setting it is MAME's own roms folder) */
            if (rp.empty()) { roms_dir_abs(full, 600); rp = full; }
            abs_path(g_set.biosDir, full, 600);
            rp += L';';
            rp += full;
        }
        if (!rp.empty()) { arg_add(a, L"-rompath"); arg_add(a, rp.c_str()); }
    }
    /* video */
    if (e.video > 0) { arg_add(a, L"-video"); arg_addf(a, L"%hs", kVideoArgs[e.video]); }
    arg_flag(a, L"window", !e.fullscreen);
    if (!e.fullscreen) arg_flag(a, L"maximize", g_set.maximize);
    if (e.resW > 0) { arg_add(a, L"-resolution"); arg_addf(a, L"%dx%d", e.resW, e.resH); }
    switch (e.rotate) {
    case 1: arg_add(a, L"-norotate"); break;
    case 2: arg_add(a, L"-ror"); break;
    case 3: arg_add(a, L"-rol"); break;
    case 4: arg_add(a, L"-flipx"); arg_add(a, L"-flipy"); break;
    }
    arg_flag(a, L"keepaspect", e.keepAspect);
    arg_flag(a, L"unevenstretch", g_set.unevenStretch);
    if (g_set.intScale > 0) { arg_add(a, L"-intscalex"); arg_addf(a, L"%d", g_set.intScale); arg_add(a, L"-intscaley"); arg_addf(a, L"%d", g_set.intScale); }
    arg_flag(a, L"filter", e.filter);
    if (g_set.prescale > 1) { arg_add(a, L"-prescale"); arg_addf(a, L"%d", g_set.prescale); }
    arg_flag(a, L"waitvsync", e.waitVsync);
    arg_flag(a, L"syncrefresh", g_set.syncRefresh);
    arg_flag(a, L"throttle", g_set.throttle);
    if (g_set.frameskip < 0) arg_add(a, L"-autoframeskip");
    else { arg_add(a, L"-noautoframeskip"); arg_add(a, L"-frameskip"); arg_addf(a, L"%d", g_set.frameskip); }
    /* HLSL belongs to the Direct3D video mode, GLSL to the OpenGL one (BGFX has its own screen effects): in the other modes they are switched off explicitly, so a mame.ini that has them on does not apply */
    arg_flag(a, L"hlsl_enable", e.video == 2 && g_set.hlsl);
    if (e.video == 2 && g_set.hlsl) add_words(a, g_set.hlslOpts);
    arg_flag(a, L"gl_glsl", e.video == 3 && g_set.glsl);
    if (e.video == 3) {
        if (g_set.glsl) {
            if (g_set.glslFilter != 1) { arg_add(a, L"-gl_glsl_filter"); arg_addf(a, L"%d", g_set.glslFilter); }
            if (g_set.glslShader[0]) { arg_add(a, L"-glsl_shader_screen0"); arg_add(a, g_set.glslShader); }
        }
    }
    if (e.chain > 0) { arg_add(a, L"-bgfx_screen_chains"); arg_addf(a, L"%hs", kChainArgs[e.chain]); }
    if (g_set.backend > 0) { arg_add(a, L"-bgfx_backend"); arg_addf(a, L"%hs", kBackendArgs[g_set.backend]); }
    arg_flag(a, L"artwork_crop", g_set.artCrop);
    /* audio */
    if (e.sound > 0) { arg_add(a, L"-sound"); arg_addf(a, L"%hs", kSoundArgs[e.sound]); }
    if (g_set.sampleRate > 0) { arg_add(a, L"-samplerate"); arg_addf(a, L"%d", g_set.sampleRate); }
    if (e.volume != 0) { arg_add(a, L"-volume"); arg_addf(a, L"%d", e.volume); }
    if (g_set.latency != 2) { arg_add(a, L"-audio_latency"); arg_addf(a, L"%d", g_set.latency); }
    /* controls */
    arg_flag(a, L"joystick", g_set.joystick);
    if (g_set.joyProvider > 0) { arg_add(a, L"-joystickprovider"); arg_addf(a, L"%hs", kProviderArgs[g_set.joyProvider]); }
    arg_flag(a, L"mouse", g_set.mouse);
    arg_flag(a, L"lightgun", g_set.lightgun);
    arg_flag(a, L"joystick_contradictory", g_set.contradictory);
    if (g_set.deadzone != 30) { arg_add(a, L"-joystick_deadzone"); arg_addf(a, L"%d.%02d", g_set.deadzone / 100, g_set.deadzone % 100); }
    if (g_set.saturation != 85) { arg_add(a, L"-joystick_saturation"); arg_addf(a, L"%d.%02d", g_set.saturation / 100, g_set.saturation % 100); }
    if (g_set.ctrlr[0]) { arg_add(a, L"-ctrlr"); arg_add(a, g_set.ctrlr); }
    /* the rest */
    arg_flag(a, L"skip_gameinfo", e.skipInfo);
    arg_flag(a, L"autosave", e.autosave);
    arg_flag(a, L"cheat", e.cheat);
    arg_flag(a, L"confirm_quit", g_set.confirmQuit);
    add_words(a, g_set.moreOpts);   /* the All MAME Options window */
    add_words(a, g_set.extra);
    if (gc) add_words(a, gc->extra);
    if (!(po && po->noFilter) && g && g->set) bind_strip_game_cfg(g->set);   /* the mapping of the launcher wins over the game's own input file */
    if (g_set.skipWarn && !(po && po->noFilter)) { arg_add(a, L"-keyboardprovider"); arg_add(a, L"dinput"); }
    g_afPlugin = 0;
    if (!(po && po->noFilter)) {   /* the plugins of the launcher: the combos, and the input log (mame-ex-input.log). MAME's own plugins stay in the path: its boot.lua is what loads ours */
        wchar_t pp[560], dd[560], both[1700];
        std::wstring names, paths = L"plugins";
        if ((g_afPlugin = combos_plugin_prepare(pp, 560)) != 0) { paths += L";"; paths += pp; names += L"mamex_combos"; }
        if (g_set.logs && diag_plugin_prepare(dd, 560)) { paths += L";"; paths += dd; if (!names.empty()) names += L","; names += L"mamex_diag"; }
        if (!names.empty()) {
            swprintf(both, 1700, L"%ls", paths.c_str());
            arg_add(a, L"-plugins"); arg_add(a, L"-pluginspath"); arg_add(a, both); arg_add(a, L"-plugin"); arg_add(a, names.c_str());
        }
    }
    if (g_set.fxOn && !(po && po->noFilter)) {   /* the filter window shows the picture: MAME gets a plain window of the picture size, unscaled and unfiltered (the later words win) */
        static const wchar_t *const fx[] = {L"-window", L"-nomaximize", L"-nokeepaspect", L"-nofilter", L"-prescale", L"1", L"-intscalex", L"0", L"-intscaley", L"0", L"-resolution", L"auto"};
        size_t k;
        for (k = 0; k < sizeof fx / sizeof fx[0]; k++) arg_add(a, fx[k]);
    }
}

/* the probe run of padprobe.cpp: the game pong without video and sound for one second, with the probe plugin (MAME's own plugins stay in the path: its boot.lua loads ours) */
void mame_probe_args(ArgList *a, const wchar_t *pluginDir) {
    wchar_t pp[1200];
    arg_add(a, L"pong");
    arg_add(a, L"-video"); arg_add(a, L"none");
    arg_add(a, L"-sound"); arg_add(a, L"none");
    arg_add(a, L"-seconds_to_run"); arg_add(a, L"2");
    arg_flag(a, L"joystick", 1);
    if (g_set.joyProvider > 0) { arg_add(a, L"-joystickprovider"); arg_addf(a, L"%hs", kProviderArgs[g_set.joyProvider]); }
    arg_flag(a, L"plugins", 1);
    swprintf(pp, 1200, L"plugins;%ls", pluginDir);
    arg_add(a, L"-pluginspath"); arg_add(a, pp);
    arg_add(a, L"-plugin"); arg_add(a, L"mamex_probe");
}

/* ---------------------------------------------------------------- starting MAME */
void mame_log_start(const ArgList *a) {
    wchar_t exe[560], logp[560], *cl;
    Buf log = {0};
    if (!g_set.logs) return;   /* Enable Logs is off: no log file at all */
    char *u;
    mame_exe(exe, 560);
    cl = command_line_join(exe, a);
    u = w_to_u8(cl);
    buf_fmt(&log, "MAME EX v" APP_VERSION "\r\n%s\r\n", u);
    free(u); free(cl);
    {   /* the configuration files MAME will read for this game */
        wchar_t md[560], cfgd[600], f1[700], f2[700];
        WIN32_FILE_ATTRIBUTE_DATA fa;
        const wchar_t *set = (a && a->n > 0) ? a->a[0] : L"";
        char *s8;
        mame_dir(md, 560);
        path_join(cfgd, 600, md, L"cfg");
        path_join(f1, 700, cfgd, L"default.cfg");
        swprintf(f2, 700, L"%ls\\%ls.cfg", cfgd, set);
        s8 = w_to_u8(f1);
        buf_fmt(&log, "default.cfg: %s %s\r\n", s8, GetFileAttributesExW(f1, GetFileExInfoStandard, &fa) ? "(exists)" : "(missing)");
        free(s8);
        s8 = w_to_u8(f2);
        buf_fmt(&log, "game cfg: %s %s%s\r\n", s8, GetFileAttributesExW(f2, GetFileExInfoStandard, &fa) ? "(exists: its input settings win over default.cfg)" : "(none)", "");
        free(s8);
    }
    log_path(logp, 560, L"mame-ex-launch.log");
    write_file(logp, log.s, log.n);
    buf_free(&log);
}

/* what MAME prints: in mame-ex-output.log with Enable Logs, otherwise in a temporary file (the launcher still shows the message when a game fails) */
static void output_path(wchar_t *out, size_t cap) {
    if (g_set.logs) { log_path(out, cap, L"mame-ex-output.log"); return; }
    { wchar_t tmp[MAX_PATH]; DWORD n = GetTempPathW(MAX_PATH, tmp); if (n && n < MAX_PATH - 30) { path_join(out, cap, tmp, L"mame-ex-output.tmp"); return; } }
    path_join(out, cap, g_root, L"mame-ex-output.tmp");
}

/* MAME's console messages (a failing video module, a missing file ...) are written to a file, so the launcher can show them when the game ends badly */
/* Skip Game Warning Screen: MAME has no option for it, the screen only waits for any key. So a key MAME does not use (F13) is sent while the game
   starts. MAME is started with -keyboardprovider dinput for this: DirectInput sees keys that are sent by a program (the default raw input module ignores
   them) and, unlike the win32 provider, still gets Alt+Enter (full screen) and the other Alt keys. */
typedef struct { DWORD pid; int n; } WarnWin;
static BOOL CALLBACK warn_find(HWND h, LPARAM l) {
    WarnWin *c = (WarnWin *)l;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == c->pid && IsWindowVisible(h)) c->n++;
    return TRUE;
}
/* Stop: MAME is asked to close its window (it then saves nvram, cfg and the autosave state like after Esc); only a MAME that does not answer is ended */
typedef struct { DWORD pid; HANDLE h; } StopCtx;
static BOOL CALLBACK stop_close(HWND h, LPARAM l) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == (DWORD)l) PostMessageW(h, WM_CLOSE, 0, 0);
    return TRUE;
}
static DWORD WINAPI stop_thread(LPVOID p) {
    StopCtx *c = (StopCtx *)p;
    EnumWindows(stop_close, (LPARAM)c->pid);
    if (WaitForSingleObject(c->h, 8000) == WAIT_TIMEOUT) TerminateProcess(c->h, 0);
    CloseHandle(c->h);
    delete c;
    return 0;
}
void mame_stop(const PROCESS_INFORMATION *pi) {
    StopCtx *c = new StopCtx;
    c->pid = pi->dwProcessId;
    if (!DuplicateHandle(GetCurrentProcess(), pi->hProcess, GetCurrentProcess(), &c->h, 0, FALSE, DUPLICATE_SAME_ACCESS)) { TerminateProcess(pi->hProcess, 0); delete c; return; }
    CloseHandle(CreateThread(NULL, 0, stop_thread, c, 0, NULL));
}

static DWORD WINAPI warn_thread(LPVOID p) {
    WarnWin c;
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)(UINT_PTR)p);
    int i;
    if (!h) return 0;
    c.pid = (DWORD)(UINT_PTR)p;
    for (i = 0; i < 40; i++) {   /* the loading of the ROMs can take a while: tap until the game runs */
        INPUT in[2];
        if (WaitForSingleObject(h, 500) != WAIT_TIMEOUT) break;
        c.n = 0;
        EnumWindows(warn_find, (LPARAM)&c);
        if (!c.n) continue;
        {   /* only while the game window has the focus: the key must not go to another program */
            DWORD fp = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &fp);
            if (fp != c.pid) continue;
        }
        memset(in, 0, sizeof in);
        in[0].type = in[1].type = INPUT_KEYBOARD;
        in[0].ki.wScan = in[1].ki.wScan = 0x64;   /* F13 */
        in[0].ki.dwFlags = KEYEVENTF_SCANCODE;
        in[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        SendInput(1, &in[0], sizeof(INPUT));
        Sleep(120);
        SendInput(1, &in[1], sizeof(INPUT));
    }
    CloseHandle(h);
    return 0;
}

BOOL mame_start(ArgList *a, PROCESS_INFORMATION *pi) {
    bind_autofire_mode(g_afPlugin);   /* the plugin of the last mame_args runs: autofire works; otherwise the buttons get their real codes */
    STARTUPINFOW si;
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    wchar_t exe[560], dir[560], outp[560], *cl;
    HANDLE f;
    BOOL ok;
    mame_exe(exe, 560);
    mame_dir(dir, 560);
    cl = command_line_join(exe, a);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    output_path(outp, 560);
    f = CreateFileW(outp, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = f;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    ok = CreateProcessW(exe, cl, NULL, NULL, f != INVALID_HANDLE_VALUE, g_set.hideConsole ? CREATE_NO_WINDOW : 0, NULL, dir, &si, pi);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    free(cl);
    if (ok && g_set.skipWarn) { HANDLE t = CreateThread(NULL, 0, warn_thread, (LPVOID)(UINT_PTR)pi->dwProcessId, 0, NULL); if (t) CloseHandle(t); }
    return ok;
}

wchar_t *mame_output_tail(size_t maxChars) {
    wchar_t outp[560], *w, *start;
    size_t len = 0, n, i, j;
    char *txt;
    output_path(outp, 560);
    txt = read_file(outp, &len);
    if (!txt) return wdup(L"");
    if (len > 8000) { memmove(txt, txt + len - 8000, 8000); txt[8000] = 0; len = 8000; }
    w = u8_to_w(txt, -1);
    free(txt);
    for (i = j = 0; w[i]; i++) if (w[i] != L'\r') w[j++] = w[i];   /* plain line ends for the message box */
    w[j] = 0;
    n = j;
    start = w;
    if (n > maxChars) { start = w + (n - maxChars); while (*start && *start != L'\n') start++; if (*start) start++; }
    start = wdup(start);
    free(w);
    trim_w(start);
    return start;
}

/* runs mame.exe hidden and collects what it prints: 0 on success, 1 no pipe, 2 could not start, 3 timeout */
int mame_run_capture(const wchar_t *args, Buf *out, DWORD timeoutMs, DWORD *exitCode) {
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE rd = NULL, wr = NULL;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t exe[560], dir[560];
    std::wstring cl;
    DWORD t0 = GetTickCount(), got;
    char tmp[16384];
    int exited = 0, ok;
    mame_exe(exe, 560);
    mame_dir(dir, 560);
    if (!CreatePipe(&rd, &wr, &sa, 0)) return 1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    cl = quote_arg(exe);
    if (args && args[0]) { cl += L' '; cl += args; }
    ok = CreateProcessW(exe, &cl[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, dir, &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return 2; }
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL)) break;
        if (avail) {
            if (ReadFile(rd, tmp, sizeof tmp, &got, NULL) && got) buf_addn(out, tmp, got);
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 30) == WAIT_OBJECT_0) {
            if (exited) break;
            exited = 1;               /* one more pass to drain what is left */
            continue;
        }
        if (GetTickCount() - t0 > timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(rd); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            return 3;
        }
    }
    if (exitCode) { *exitCode = 0; GetExitCodeProcess(pi.hProcess, exitCode); }
    CloseHandle(rd); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}

/* ---------------------------------------------------------------- frontends */
typedef struct { DWORD pid; HWND found; } FindCtx;
static BOOL CALLBACK find_proc(HWND h, LPARAM lp) {
    FindCtx *c = (FindCtx *)lp;
    DWORD p = 0;
    GetWindowThreadProcessId(h, &p);
    if (p == c->pid && IsWindowVisible(h)) { c->found = h; return FALSE; }
    return TRUE;
}

/* gives the keyboard focus to the first visible window of a process (a start from a frontend has no window of its own that could
   hand the focus over, so MAME may otherwise start in the background without receiving input) */
static DWORD WINAPI focus_thread(LPVOID pidp) {
    FindCtx c;
    int tries;
    c.pid = (DWORD)(size_t)pidp;
    for (tries = 0; tries < 60; tries++) {
        Sleep(250);
        c.found = NULL;
        EnumWindows(find_proc, (LPARAM)&c);
        if (c.found) {
            keybd_event(VK_MENU, 0, 0, 0);   /* a tap of Alt lifts the foreground lock */
            keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
            SetForegroundWindow(c.found);
            return 0;
        }
    }
    return 0;
}

/* "MAME-EX.exe --game <set name or ROM file> [--fullscreen | --windowed]": for frontends (LaunchBox, Pegasus, RetroBat / EmulationStation, Playnite, Steam...).
 * Starts the game with the global and the game's own settings, without the launcher window, waits until the game is closed and then returns.
 * Exit codes: 0 played, 2 unknown game, 3 ROM files missing, 4 MAME could not be started, 5 no game given. Problems are written to mame-ex-launch.log. */
static void cli_log(const wchar_t *what, const wchar_t *arg) {
    wchar_t logp[560];
    Buf log = {0};
    char *w8, *a8;
    if (!g_set.logs) return;
    w8 = w_to_u8(what); a8 = w_to_u8(arg ? arg : L"");
    buf_fmt(&log, "%s: %s\r\n", w8, a8);
    log_path(logp, 560, L"mame-ex-launch.log");
    write_file(logp, log.s, log.n);
    buf_free(&log);
    free(w8); free(a8);
}

static const Game *cli_find_game(const wchar_t *arg) {
    wchar_t name[300], *p, *dot;
    int i, pass;
    wcsncpy(name, arg, 299);
    name[299] = 0;
    trim_w(name);
    if (name[0] == L'"') memmove(name, name + 1, wcslen(name) * sizeof(wchar_t));
    { size_t l = wcslen(name); if (l && name[l - 1] == L'"') name[l - 1] = 0; }
    p = wcsrchr(name, L'\\');
    if (wcsrchr(name, L'/') > p) p = wcsrchr(name, L'/');
    if (p) memmove(name, p + 1, (wcslen(p + 1) + 1) * sizeof(wchar_t));
    for (pass = 0; pass < 2; pass++) {   /* first the name as it is, then without the extension (.zip, .7z...) */
        if (pass == 1) { dot = wcsrchr(name, L'.'); if (!dot) break; *dot = 0; }
        for (i = 0; i < g_ngames; i++) if (g_games[i].set && !_wcsicmp(g_games[i].set, name)) return &g_games[i];
    }
    return NULL;
}

int run_game_cli(const wchar_t *game, int fullscreen) {   /* fullscreen: -1 as set, 0 window, 1 fullscreen */
    ArgList a;
    PROCESS_INFORMATION pi;
    PlayOpts po;
    const Game *g;
    wchar_t exe[560];
    if (!game || !game[0]) { cli_log(L"no game given", L""); return 5; }
    if (!mame_exe(exe, 560)) { cli_log(L"mame.exe not found (set it in the launcher, or put it beside the launcher)", exe); return 4; }
    games_load(&g_games, &g_ngames);
    g = cli_find_game(game);
    if (!g) { cli_log(L"unknown game (use the MAME set name, e.g. sfex2, or the ROM zip of a game of the listed boards)", game); return 2; }
    {
        wchar_t rd[560];
        if (g_set.roms[0]) wcscpy(rd, g_set.roms); else roms_dir_abs(rd, 560);
        rom_available_bios(g_set.biosDir);
        rom_available_init(rd);
    }
    if (!rom_available(g)) {
        wchar_t miss[300];
        rom_missing(g, miss, 300);
        cli_log(L"ROM files missing", miss);
        return 3;
    }
    memset(&po, 0, sizeof po);
    po.fullscreen = fullscreen;
    mame_args(g->id, &po, &a);
    mame_log_start(&a);
    AllowSetForegroundWindow((DWORD)-1);
    if (!mame_start(&a, &pi)) { cli_log(L"mame.exe could not be started", exe); args_free(&a); return 4; }
    CloseHandle(CreateThread(NULL, 0, focus_thread, (LPVOID)(size_t)pi.dwProcessId, 0, NULL));
    if (g_set.fxOn) filter_start(pi.dwProcessId);
    WaitForSingleObject(pi.hProcess, INFINITE);
    filter_stop();
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    args_free(&a);
    return 0;
}
