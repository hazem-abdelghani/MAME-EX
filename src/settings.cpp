/* the launcher's settings (mame-ex-settings.cfg): everything that is passed to MAME, and how the launcher looks */
#include "common.h"

Settings g_set;
WinRect g_winRect, g_optRect;
wchar_t g_root[560], g_settingsFile[560];

/* the choices of the combo boxes; the order is the order of the values stored in the settings */
const wchar_t *const kVideoModes[5] = {L"Auto (MAME's Choice)", L"BGFX", L"Direct3D", L"OpenGL", L"GDI (software)"};
const char *const kVideoArgs[5] = {"auto", "bgfx", "d3d", "opengl", "gdi"};
const wchar_t *const kChains[6] = {L"Default", L"Unfiltered", L"HLSL (CRT simulation)", L"CRT Geometry", L"CRT Geometry Deluxe", L"LCD Grid"};
const char *const kChainArgs[6] = {"", "unfiltered", "hlsl", "crt-geom", "crt-geom-deluxe", "lcd-grid"};
const wchar_t *const kBackends[6] = {L"Default", L"Direct3D 11", L"Direct3D 12", L"OpenGL", L"Vulkan", L"Automatic"};
const char *const kBackendArgs[6] = {"", "d3d11", "d3d12", "opengl", "vulkan", "auto"};
const wchar_t *const kRotations[5] = {L"Game Default", L"None", L"Rotate Right (90°)", L"Rotate Left (90°)", L"180°"};
const wchar_t *const kSoundModes[5] = {L"Auto (MAME's Choice)", L"Off", L"DirectSound", L"XAudio2", L"PortAudio"};
const char *const kSoundArgs[5] = {"auto", "none", "dsound", "xaudio2", "portaudio"};
const wchar_t *const kProviders[5] = {L"Auto", L"Windows (hybrid)", L"DirectInput", L"XInput", L"None"};
const char *const kProviderArgs[5] = {"auto", "winhybrid", "dinput", "xinput", "none"};
const int kSampleRates[5] = {0, 22050, 32000, 44100, 48000};   /* 0 = MAME's default */

/* ---------------------------------------------------------------- theme */
static int system_dark(void) {   /* the "apps" colour mode of Windows */
    HKEY k;
    DWORD v = 1, n = sizeof v;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"AppsUseLightTheme", NULL, NULL, (BYTE *)&v, &n);
        RegCloseKey(k);
    }
    return v == 0;
}
void settings_resolve_theme(void) { g_set.dark = g_set.themeMode == 2 || (g_set.themeMode == 0 && system_dark()); }

/* ---------------------------------------------------------------- defaults */
/* the values are the defaults of MAME itself, so a fresh launcher changes nothing about how MAME behaves */
void settings_defaults(Settings *s, const Settings *keep) {
    Settings n;
    memset(&n, 0, sizeof n);
    n.hideConsole = 1;
    n.hideSearch = 1;   /* the search bar, the filter bar and the classic (tabs in the main window) layout are off at the first start */
    n.hideFilters = 1;
    n.optWindow = 1;
    n.video = 0;
    n.keepAspect = 1;
    n.unevenStretch = 1;
    n.filter = 1;
    n.prescale = 1;
    n.glslFilter = 1;
    n.fxScaler = 2;
    n.throttle = 1;
    n.sampleRate = 0;
    n.latency = 2;
    n.joystick = 1;
    n.ovGameCfg = 1;
    n.afRate = 1;
    n.deadzone = 30;
    n.saturation = 85;
    n.skipInfo = 0;
    n.sortCol = 1;
    n.sortAsc = 1;
    n.colsHidden = 8;   /* the Info column is hidden by default */
    n.themeMode = 0;
    if (keep) {   /* "Restore Defaults": the program paths, the favorites and the window layout stay */
        wcscpy(n.mame, keep->mame);
        wcscpy(n.roms, keep->roms); wcscpy(n.biosDir, keep->biosDir);
        n.onlyAvail = keep->onlyAvail; n.favOnly = keep->favOnly;
        n.dark = keep->dark; n.themeMode = keep->themeMode;
        n.hideSearch = keep->hideSearch; n.hideFilters = keep->hideFilters; n.hideToolbar = keep->hideToolbar;
        n.colsHidden = keep->colsHidden; n.optWindow = keep->optWindow;
        n.sortCol = keep->sortCol; n.sortAsc = keep->sortAsc;
        memcpy(n.colsGame, keep->colsGame, sizeof n.colsGame); n.splitX = keep->splitX;
        wcscpy(n.fRegion, keep->fRegion); wcscpy(n.fMaker, keep->fMaker); wcscpy(n.fGenre, keep->fGenre); wcscpy(n.fHw, keep->fHw);
        il_copy(&n.fav, &keep->fav);
        il_copy(&n.recent, &keep->recent);
        /* the Controls tab (and the Combos tab, which has a file of its own) are not touched */
        n.joystick = keep->joystick; n.joyProvider = keep->joyProvider; n.mouse = keep->mouse; n.lightgun = keep->lightgun;
        n.deadzone = keep->deadzone; n.saturation = keep->saturation; n.contradictory = keep->contradictory; n.ovGameCfg = keep->ovGameCfg;
        wcscpy(n.ctrlr, keep->ctrlr); wcscpy(n.profile, keep->profile);
        n.bindVer = keep->bindVer; n.afRate = keep->afRate;
        {   /* the light gun words of the Controls tab live in the free option string */
            wchar_t *p, *ctx = NULL, tmp[2048];
            wcsncpy(tmp, keep->moreOpts, 2047); tmp[2047] = 0;
            for (p = wcstok_s(tmp, L" ", &ctx); p; p = wcstok_s(NULL, L" ", &ctx))
                if (!wcscmp(p, L"-lightgun_device") || !wcscmp(p, L"-lightgunprovider")) {
                    wchar_t *v = wcstok_s(NULL, L" ", &ctx);
                    if (v && wcslen(n.moreOpts) + wcslen(p) + wcslen(v) + 3 < sizeof n.moreOpts / sizeof(wchar_t)) { if (n.moreOpts[0]) wcscat(n.moreOpts, L" "); wcscat(n.moreOpts, p); wcscat(n.moreOpts, L" "); wcscat(n.moreOpts, v); }
                }
        }
    }
    il_clear(&s->fav);
    il_clear(&s->recent);
    *s = n;
    s->dark = n.themeMode == 2 || (n.themeMode == 0 && system_dark());
}

typedef struct { const char *key; int kind; void *p; int cap; } IniField;   /* kind 0: text, 1: number, 2: flag */

static int ini_fields(IniField *f) {
    Settings *s = &g_set;
    int n = 0;
#define TXT(k, v) f[n].key = k, f[n].kind = 0, f[n].p = v, f[n].cap = (int)(sizeof v / sizeof(wchar_t)), n++
#define NUM(k, v) f[n].key = k, f[n].kind = 1, f[n].p = &(v), n++
#define FLG(k, v) f[n].key = k, f[n].kind = 2, f[n].p = &(v), n++
    TXT("mame_exe", s->mame); TXT("roms_directory", s->roms); TXT("bios_directory", s->biosDir);
    FLG("hide_console", s->hideConsole); NUM("on_game_start", s->onStart); NUM("binding_version", s->bindVer); FLG("logs", s->logs);
    NUM("video", s->video); FLG("fullscreen", s->fullscreen); NUM("res_w", s->resW); NUM("res_h", s->resH); NUM("rotate", s->rotate);
    FLG("keep_aspect", s->keepAspect); FLG("uneven_stretch", s->unevenStretch); FLG("filter", s->filter); NUM("prescale", s->prescale);
    FLG("wait_vsync", s->waitVsync); FLG("sync_refresh", s->syncRefresh); FLG("throttle", s->throttle); NUM("frameskip", s->frameskip); FLG("maximize", s->maximize);
    NUM("bgfx_chain", s->chain); NUM("bgfx_backend", s->backend);
    FLG("artwork_crop", s->artCrop); FLG("hlsl_enable", s->hlsl); FLG("gl_glsl", s->glsl); NUM("integer_scale", s->intScale);
    FLG("filter_window", s->fxOn); NUM("filter_scaler", s->fxScaler); FLG("filter_fxaa", s->fxFxaa); NUM("filter_dither", s->fxDither); NUM("gl_glsl_filter", s->glslFilter); TXT("glsl_shader", s->glslShader);
    NUM("sound", s->sound); NUM("sample_rate", s->sampleRate); NUM("volume", s->volume); NUM("audio_latency", s->latency);
    FLG("joystick", s->joystick); NUM("joystick_provider", s->joyProvider); FLG("mouse", s->mouse); FLG("lightgun", s->lightgun);
    NUM("joystick_deadzone", s->deadzone); NUM("joystick_saturation", s->saturation); FLG("joystick_contradictory", s->contradictory); FLG("mapping_overrides_game_cfg", s->ovGameCfg); NUM("autofire_rate", s->afRate); TXT("controller_file", s->ctrlr); TXT("hlsl_options", s->hlslOpts); TXT("more_options", s->moreOpts); TXT("controls_profile", s->profile);
    FLG("skip_gameinfo", s->skipInfo); FLG("skip_warnings", s->skipWarn); FLG("autosave", s->autosave); FLG("cheat", s->cheat); FLG("confirm_quit", s->confirmQuit); TXT("extra_arguments", s->extra);
    FLG("only_available", s->onlyAvail); FLG("dark_theme", s->dark);
    return n;
}

static void parse_ints(const char *v, int *arr, int n) {
    char *copy = _strdup(v), *tok, *save = NULL;
    int i = 0;
    for (tok = strtok_s(copy, ",", &save); tok && i < n; tok = strtok_s(NULL, ",", &save), i++) { int x; if (xatoi(trim_a(tok), &x) && x >= 0 && x < 4000) arr[i] = x; }
    free(copy);
}

static void add_ints(Buf *b, const char *name, const int *arr, int n) {
    int i, any = 0;
    for (i = 0; i < n; i++) if (arr[i] > 0) any = 1;
    if (!any) return;
    buf_fmt(b, "%s=", name);
    for (i = 0; i < n; i++) buf_fmt(b, "%s%d", i ? "," : "", arr[i]);
    buf_add(b, "\r\n");
}

static void parse_int_list(const char *v, IntList *l) {
    char *copy = _strdup(v), *tok, *save = NULL;
    il_clear(l);
    for (tok = strtok_s(copy, ",", &save); tok; tok = strtok_s(NULL, ",", &save)) {
        int x;
        if (xatoi(trim_a(tok), &x)) il_add(l, x);
    }
    free(copy);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void settings_load(void) {
    char *txt = read_file(g_settingsFile, NULL), *line, *save = NULL;
    IniField f[64];
    int nf = ini_fields(f), i, haveFav = 0, haveRecent = 0;
    char favs[4096] = "", recents[512] = "";
    int wx = 0, wy = 0, ww = 0, wh = 0;
    if (!txt) { settings_resolve_theme(); return; }
    for (line = strtok_s(txt, "\n", &save); line; line = strtok_s(NULL, "\n", &save)) {
        char *eq, *key, *val;
        line = trim_a(line);
        if (!*line || *line == ';' || *line == '#' || *line == '[') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        key = trim_a(line);
        val = trim_a(eq + 1);
        _strlwr(key);
        for (i = 0; i < nf; i++) {
            if (strcmp(f[i].key, key)) continue;
            if (f[i].kind == 0) { wchar_t *w = u8_to_w(val, -1); wcsncpy((wchar_t *)f[i].p, w, f[i].cap - 1); ((wchar_t *)f[i].p)[f[i].cap - 1] = 0; free(w); }
            else if (f[i].kind == 1) { int x; if (xatoi(val, &x)) *(int *)f[i].p = x; }
            else *(int *)f[i].p = !strcmp(val, "1") || !_stricmp(val, "yes") || !_stricmp(val, "true");
        }
        if (!strcmp(key, "theme_mode")) xatoi(val, &g_set.themeMode);
        else if (!strcmp(key, "search_bar")) g_set.hideSearch = !strcmp(val, "0");
        else if (!strcmp(key, "filters_bar")) g_set.hideFilters = !strcmp(val, "0");
        else if (!strcmp(key, "toolbar")) g_set.hideToolbar = !strcmp(val, "0");
        else if (!strcmp(key, "favorites_only")) g_set.favOnly = !strcmp(val, "1");
        else if (!strcmp(key, "filter_region")) { wchar_t *w = u8_to_w(val, -1); wcsncpy(g_set.fRegion, w, 7); free(w); }
        else if (!strcmp(key, "filter_maker")) { wchar_t *w = u8_to_w(val, -1); wcsncpy(g_set.fMaker, w, 39); free(w); }
        else if (!strcmp(key, "filter_hardware")) { wchar_t *w = u8_to_w(val, -1); wcsncpy(g_set.fHw, w, 39); free(w); }
        else if (!strcmp(key, "filter_genre")) { wchar_t *w = u8_to_w(val, -1); wcsncpy(g_set.fGenre, w, 23); free(w); }
        else if (!strcmp(key, "sort_column")) xatoi(val, &g_set.sortCol);
        else if (!strcmp(key, "sort_ascending")) g_set.sortAsc = !strcmp(val, "1");
        else if (!strcmp(key, "columns_games")) parse_ints(val, g_set.colsGame, 7);
        else if (!strcmp(key, "splitter")) xatoi(val, &g_set.splitX);
        else if (!strcmp(key, "columns_hidden")) xatoi(val, &g_set.colsHidden);
        else if (!strcmp(key, "window_x")) xatoi(val, &wx);
        else if (!strcmp(key, "window_y")) xatoi(val, &wy);
        else if (!strcmp(key, "window_w")) xatoi(val, &ww);
        else if (!strcmp(key, "window_h")) xatoi(val, &wh);
        else if (!strcmp(key, "options_window")) g_set.optWindow = !strcmp(val, "1");
        else if (!strcmp(key, "optwin_x")) xatoi(val, &g_optRect.x);
        else if (!strcmp(key, "optwin_y")) xatoi(val, &g_optRect.y);
        else if (!strcmp(key, "optwin_w")) xatoi(val, &g_optRect.w);
        else if (!strcmp(key, "optwin_h")) xatoi(val, &g_optRect.h);
        else if (!strcmp(key, "favorites")) { strncpy(favs, val, sizeof favs - 1); haveFav = 1; }
        else if (!strcmp(key, "recent")) { strncpy(recents, val, sizeof recents - 1); haveRecent = 1; }
    }
    free(txt);
    if (g_set.themeMode < 0 || g_set.themeMode > 2) g_set.themeMode = 0;
    settings_resolve_theme();
    if (ww > 0) { g_winRect.w = ww; g_winRect.x = wx; g_winRect.y = wy; g_winRect.h = wh; }
    if (haveFav) parse_int_list(favs, &g_set.fav);
    if (haveRecent) parse_int_list(recents, &g_set.recent);
    g_set.sortCol = clampi(g_set.sortCol, 0, 6);
    if (g_set.sortCol == 3) g_set.sortCol = 1;
    g_set.onStart = clampi(g_set.onStart, 0, 2);
    g_set.colsHidden &= 127;
    g_set.optWindow = g_set.optWindow != 0;
    g_set.video = clampi(g_set.video, 0, 4);
    g_set.rotate = clampi(g_set.rotate, 0, 4);
    g_set.chain = clampi(g_set.chain, 0, 5);
    g_set.backend = clampi(g_set.backend, 0, 5);
    g_set.sound = clampi(g_set.sound, 0, 4);
    g_set.joyProvider = clampi(g_set.joyProvider, 0, 4);
    g_set.prescale = clampi(g_set.prescale, 1, 8);
    g_set.glslFilter = clampi(g_set.glslFilter, 0, 2);
    g_set.intScale = clampi(g_set.intScale, 0, 8);
    g_set.fxScaler = clampi(g_set.fxScaler, 0, 4);
    g_set.fxDither = clampi(g_set.fxDither, 0, 3);
    g_set.frameskip = clampi(g_set.frameskip, -1, 10);
    g_set.volume = clampi(g_set.volume, -32, 0);
    g_set.latency = clampi(g_set.latency, 1, 5);
    g_set.deadzone = clampi(g_set.deadzone, 0, 100);
    g_set.saturation = clampi(g_set.saturation, 0, 100);
}

static void add_list(Buf *b, const char *name, const IntList *l) {
    int i;
    buf_fmt(b, "%s=", name);
    for (i = 0; i < l->n; i++) buf_fmt(b, "%s%d", i ? "," : "", l->v[i]);
    buf_add(b, "\r\n");
}

int settings_save(void) {
    Buf b = {0};
    IniField f[64];
    int nf = ini_fields(f), i, ok;
    buf_add(&b, "; MAME EX settings (written by mame-ex; safe to edit by hand)\r\n[MAME-EX]\r\nsettings_version=1\r\n");
    for (i = 0; i < nf; i++) {
        if (f[i].kind == 0) { char *u = w_to_u8((wchar_t *)f[i].p); buf_fmt(&b, "%s=%s\r\n", f[i].key, u); free(u); }
        else buf_fmt(&b, "%s=%d\r\n", f[i].key, f[i].kind == 2 ? *(int *)f[i].p != 0 : *(int *)f[i].p);
    }
    if (g_winRect.w > 0) buf_fmt(&b, "window_x=%d\r\nwindow_y=%d\r\nwindow_w=%d\r\nwindow_h=%d\r\n", g_winRect.x, g_winRect.y, g_winRect.w, g_winRect.h);
    buf_fmt(&b, "options_window=%d\r\ntheme_mode=%d\r\nsearch_bar=%d\r\nfilters_bar=%d\r\ntoolbar=%d\r\n", g_set.optWindow != 0, g_set.themeMode, !g_set.hideSearch, !g_set.hideFilters, !g_set.hideToolbar);
    if (g_optRect.w > 0) buf_fmt(&b, "optwin_x=%d\r\noptwin_y=%d\r\noptwin_w=%d\r\noptwin_h=%d\r\n", g_optRect.x, g_optRect.y, g_optRect.w, g_optRect.h);
    buf_fmt(&b, "favorites_only=%d\r\nsort_column=%d\r\nsort_ascending=%d\r\n", g_set.favOnly != 0, g_set.sortCol, g_set.sortAsc != 0);
    { char *u;
      u = w_to_u8(g_set.fRegion); buf_fmt(&b, "filter_region=%s\r\n", u); free(u);
      u = w_to_u8(g_set.fMaker); buf_fmt(&b, "filter_maker=%s\r\n", u); free(u);
      u = w_to_u8(g_set.fGenre); buf_fmt(&b, "filter_genre=%s\r\n", u); free(u);
      u = w_to_u8(g_set.fHw); buf_fmt(&b, "filter_hardware=%s\r\n", u); free(u); }
    add_ints(&b, "columns_games", g_set.colsGame, 7);
    if (g_set.splitX > 0) buf_fmt(&b, "splitter=%d\r\n", g_set.splitX);
    buf_fmt(&b, "columns_hidden=%d\r\n", g_set.colsHidden & 127);
    add_list(&b, "favorites", &g_set.fav);
    add_list(&b, "recent", &g_set.recent);
    ok = write_file(g_settingsFile, b.s, b.n);
    buf_free(&b);
    return ok;
}

void settings_sig(Buf *b) {
    IniField f[64];
    int nf = ini_fields(f), i;
    for (i = 0; i < nf; i++) {
        if (!strcmp(f[i].key, "dark_theme")) continue;   /* the theme is chosen in the View menu and saved at once */
        if (f[i].kind == 0) { char *u = w_to_u8((wchar_t *)f[i].p); buf_fmt(b, "%s;", u); free(u); }
        else buf_fmt(b, "%d;", *(int *)f[i].p);
    }
}

void set_favorite(int id, int on) {
    il_remove(&g_set.fav, id);
    if (on) il_add(&g_set.fav, id);
    settings_save();
}
