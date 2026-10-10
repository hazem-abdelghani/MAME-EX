/* The HLSL settings of MAME's Direct3D video mode: the options of the "HLSL post-processing" part of mame.ini (the files bloom.fx, chroma.fx ... in MAME's
   hlsl folder are the shaders; these options are their parameters). Only the values that differ from MAME's defaults are kept, as command line words
   in Settings.hlslOpts ("-scanline_alpha 0.7 -shadow_mask_alpha 0.4") and added to the command line when HLSL is on. */
#include "common.h"
#include <string>
#include <vector>

typedef struct { const wchar_t *group, *label; const char *name, *def; char kind; double lo, hi; } HOpt;   /* kind: F float, I integer, B on / off, S comma separated numbers, T a file name */
static const HOpt kOpts[] = {
    {L"Scanlines", L"Alpha (0 = off)", "scanline_alpha", "0.0", 'F', 0, 4}, {L"Scanlines", L"Height Scale", "scanline_size", "1.0", 'F', 0, 4}, {L"Scanlines", L"Line Height", "scanline_height", "1.0", 'F', 0, 4},
    {L"Scanlines", L"Variation", "scanline_variation", "1.0", 'F', 0, 4}, {L"Scanlines", L"Brightness Scale", "scanline_bright_scale", "1.0", 'F', 0, 2}, {L"Scanlines", L"Brightness Offset", "scanline_bright_offset", "0.0", 'F', 0, 1},
    {L"Scanlines", L"Jitter", "scanline_jitter", "0.0", 'F', 0, 4}, {L"Scanlines", L"Hum Bar Alpha", "hum_bar_alpha", "0.0", 'F', 0, 1},
    {L"Shadow Mask", L"Alpha (0 = off)", "shadow_mask_alpha", "0.0", 'F', 0, 1}, {L"Shadow Mask", L"Tile Mode (0 screen, 1 source)", "shadow_mask_tile_mode", "0", 'I', 0, 1},
    {L"Shadow Mask", L"Tile Width", "shadow_mask_x_count", "6", 'I', 1, 4096}, {L"Shadow Mask", L"Tile Height", "shadow_mask_y_count", "4", 'I', 1, 4096},
    {L"Shadow Mask", L"Texture Width (U)", "shadow_mask_usize", "0.1875", 'F', 0, 1}, {L"Shadow Mask", L"Texture Height (V)", "shadow_mask_vsize", "0.25", 'F', 0, 1},
    {L"Shadow Mask", L"Offset U", "shadow_mask_uoffset", "0.0", 'F', -1, 1}, {L"Shadow Mask", L"Offset V", "shadow_mask_voffset", "0.0", 'F', -1, 1},
    {L"Screen Shape", L"Distortion", "distortion", "0.0", 'F', -1, 1}, {L"Screen Shape", L"Cubic Distortion", "cubic_distortion", "0.0", 'F', -1, 1},
    {L"Screen Shape", L"Distort Corner", "distort_corner", "0.0", 'F', 0, 1}, {L"Screen Shape", L"Round Corner", "round_corner", "0.0", 'F', 0, 1},
    {L"Screen Shape", L"Smooth Border", "smooth_border", "0.0", 'F', 0, 1}, {L"Screen Shape", L"Reflection", "reflection", "0.0", 'F', 0, 1}, {L"Screen Shape", L"Vignetting", "vignetting", "0.0", 'F', 0, 1},
    {L"Bloom", L"Scale (0 = off)", "bloom_scale", "0.0", 'F', 0, 10}, {L"Bloom", L"Blend Mode (0 brighten, 1 darken)", "bloom_blend_mode", "0", 'I', 0, 1}, {L"Bloom", L"Overdrive (R,G,B)", "bloom_overdrive", "1.0,1.0,1.0", 'S', 0, 0},
    {L"Bloom", L"Level 0 Weight", "bloom_lvl0_weight", "1.0", 'F', 0, 4}, {L"Bloom", L"Level 1 Weight", "bloom_lvl1_weight", "0.64", 'F', 0, 4}, {L"Bloom", L"Level 2 Weight", "bloom_lvl2_weight", "0.32", 'F', 0, 4},
    {L"Bloom", L"Level 3 Weight", "bloom_lvl3_weight", "0.16", 'F', 0, 4}, {L"Bloom", L"Level 4 Weight", "bloom_lvl4_weight", "0.08", 'F', 0, 4}, {L"Bloom", L"Level 5 Weight", "bloom_lvl5_weight", "0.06", 'F', 0, 4},
    {L"Bloom", L"Level 6 Weight", "bloom_lvl6_weight", "0.04", 'F', 0, 4}, {L"Bloom", L"Level 7 Weight", "bloom_lvl7_weight", "0.02", 'F', 0, 4}, {L"Bloom", L"Level 8 Weight", "bloom_lvl8_weight", "0.01", 'F', 0, 4},
    {L"Colour", L"Saturation", "saturation", "1.0", 'F', 0, 4}, {L"Colour", L"Offset (R,G,B)", "offset", "0.0,0.0,0.0", 'S', 0, 0}, {L"Colour", L"Scale (R,G,B)", "scale", "1.0,1.0,1.0", 'S', 0, 0},
    {L"Colour", L"Power (R,G,B)", "power", "1.0,1.0,1.0", 'S', 0, 0}, {L"Colour", L"Floor (R,G,B)", "floor", "0.0,0.0,0.0", 'S', 0, 0},
    {L"Colour", L"Red Output (from R,G,B)", "red_ratio", "1.0,0.0,0.0", 'S', 0, 0}, {L"Colour", L"Green Output (from R,G,B)", "grn_ratio", "0.0,1.0,0.0", 'S', 0, 0}, {L"Colour", L"Blue Output (from R,G,B)", "blu_ratio", "0.0,0.0,1.0", 'S', 0, 0},
    {L"Phosphor And Focus", L"Phosphor Decay (R,G,B)", "phosphor_life", "0.0,0.0,0.0", 'S', 0, 0}, {L"Phosphor And Focus", L"Defocus (X,Y)", "defocus", "0.0,0.0", 'S', 0, 0},
    {L"Phosphor And Focus", L"Convergence X (R,G,B)", "converge_x", "0.0,0.0,0.0", 'S', 0, 0}, {L"Phosphor And Focus", L"Convergence Y (R,G,B)", "converge_y", "0.0,0.0,0.0", 'S', 0, 0},
    {L"Phosphor And Focus", L"Radial Convergence X (R,G,B)", "radial_converge_x", "0.0,0.0,0.0", 'S', 0, 0}, {L"Phosphor And Focus", L"Radial Convergence Y (R,G,B)", "radial_converge_y", "0.0,0.0,0.0", 'S', 0, 0},
    {L"NTSC (YIQ)", L"Enable", "yiq_enable", "0", 'B', 0, 1}, {L"NTSC (YIQ)", L"Jitter", "yiq_jitter", "0.0", 'F', -100, 100}, {L"NTSC (YIQ)", L"Colour Carrier", "yiq_cc", "3.57954545", 'F', 0, 100},
    {L"NTSC (YIQ)", L"A", "yiq_a", "0.5", 'F', -100, 100}, {L"NTSC (YIQ)", L"B", "yiq_b", "0.5", 'F', -100, 100}, {L"NTSC (YIQ)", L"O", "yiq_o", "0.0", 'F', -100, 100},
    {L"NTSC (YIQ)", L"P", "yiq_p", "1.0", 'F', -100, 100}, {L"NTSC (YIQ)", L"N", "yiq_n", "1.0", 'F', -100, 100}, {L"NTSC (YIQ)", L"Y", "yiq_y", "6.0", 'F', -100, 100},
    {L"NTSC (YIQ)", L"I", "yiq_i", "1.2", 'F', -100, 100}, {L"NTSC (YIQ)", L"Q", "yiq_q", "0.6", 'F', -100, 100}, {L"NTSC (YIQ)", L"Scan Time (us)", "yiq_scan_time", "52.6", 'F', 0, 1000},
    {L"NTSC (YIQ)", L"Phase Count", "yiq_phase_count", "2", 'I', 1, 16},
    {L"Chroma", L"Phosphors (1 mono, 2 dichrome, 3 colour)", "chroma_mode", "3", 'I', 1, 3}, {L"Chroma", L"Conversion Gain (R,G,B)", "chroma_conversion_gain", "0.299,0.587,0.114", 'S', 0, 0},
    {L"Chroma", L"First Phosphor (x,y)", "chroma_a", "0.64,0.33", 'S', 0, 0}, {L"Chroma", L"Second Phosphor (x,y)", "chroma_b", "0.30,0.60", 'S', 0, 0},
    {L"Chroma", L"Third Phosphor (x,y)", "chroma_c", "0.15,0.06", 'S', 0, 0}, {L"Chroma", L"Phosphor Gain (R,G,B)", "chroma_y_gain", "0.2126,0.7152,0.0722", 'S', 0, 0},
    {L"Shadow Mask", L"Texture File", "shadow_mask_texture", "shadow-mask.png", 'T', 0, 0},
    {L"LUT", L"Enable 3D LUT", "lut_enable", "0", 'B', 0, 1}, {L"LUT", L"LUT Texture File", "lut_texture", "lut-default.png", 'T', 0, 0},
    {L"Other", L"Oversampling", "hlsl_oversampling", "0", 'B', 0, 1},
};
#define NOPT ((int)(sizeof kOpts / sizeof kOpts[0]))

/* presets: the values that differ from the defaults (a starting point to tune by hand) */
static const struct { const wchar_t *name; const char *words; } kPresets[] = {
    {L"None (MAME's Defaults)", ""},
    {L"Light Scanlines", "-scanline_alpha 0.4"},
    {L"Scanlines + Soft Glow", "-scanline_alpha 0.5 -bloom_scale 0.25"},
    {L"CRT (Shadow Mask And Curve)", "-scanline_alpha 0.6 -shadow_mask_alpha 0.35 -distortion 0.05 -round_corner 0.03 -vignetting 0.2"},
    {L"Arcade Monitor", "-scanline_alpha 0.55 -shadow_mask_alpha 0.3 -bloom_scale 0.3 -defocus 0.4,0.4 -vignetting 0.2 -saturation 1.15"},
};

typedef struct { std::string val[NOPT]; HWND lv, cbPre; } HCtx;

static std::wstring wa(const char *s) { wchar_t *w = u8_to_w(s, -1); std::wstring r = w; free(w); return r; }

static void parse_words(const char *text, HCtx *c) {   /* "-name value ..." into the values */
    std::string t = text, tok;
    std::vector<std::string> v;
    size_t p = 0;
    int i;
    for (i = 0; i < NOPT; i++) c->val[i] = kOpts[i].def;
    while (p < t.size()) {
        size_t e = t.find(' ', p);
        tok = t.substr(p, e == std::string::npos ? std::string::npos : e - p);
        p = e == std::string::npos ? t.size() : e + 1;
        if (!tok.empty()) v.push_back(tok);
    }
    for (size_t k = 0; k + 1 < v.size(); k++) {
        if (v[k][0] != '-') continue;
        for (i = 0; i < NOPT; i++) if (v[k].substr(1) == kOpts[i].name) { c->val[i] = v[k + 1]; k++; break; }
    }
}

static std::string words_of(const HCtx *c) {   /* only what differs from the defaults */
    std::string out;
    for (int i = 0; i < NOPT; i++) if (c->val[i] != kOpts[i].def) out += std::string(out.empty() ? "" : " ") + "-" + kOpts[i].name + " " + c->val[i];
    return out;
}

static void fill_row(HCtx *c, int i) {
    std::wstring v = wa(c->val[i].c_str());
    if (kOpts[i].kind == 'B') v = c->val[i] == "1" ? L"On" : L"Off";
    ListView_SetItemText(c->lv, i, 2, (LPWSTR)v.c_str());
}

static int valid_value(const HOpt &o, std::string &v) {   /* trims and checks the text typed for an option */
    size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
    v = a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
    if (v.empty()) return 0;
    if (o.kind == 'T') return v.find(' ') == std::string::npos;   /* a file name (no blanks: it is passed as one word) */
    if (o.kind == 'S') {   /* numbers separated by commas, as many as the default has */
        size_t want = 1, got = 1, p;
        const char *d = o.def;
        for (p = 0; d[p]; p++) if (d[p] == ',') want++;
        for (p = 0; p < v.size(); p++) { if (v[p] == ',') got++; else if (!strchr("0123456789.-+", v[p])) return 0; }
        return got == want;
    }
    {
        char *e = NULL;
        double x = strtod(v.c_str(), &e);
        if (!e || *e) return 0;
        if (o.kind == 'B') return v == "0" || v == "1";
        if (o.kind == 'I' && v.find_first_of(".eE") != std::string::npos) return 0;   /* MAME rejects 2.5 for an integer option */
        return x >= o.lo && x <= o.hi;
    }
}

static void edit_row(Modal *md, int i) {
    HCtx *c = (HCtx *)md->user;
    if (i < 0 || i >= NOPT) return;
    if (kOpts[i].kind == 'B') c->val[i] = c->val[i] == "1" ? "0" : "1";
    else {
        wchar_t t[120], prompt[200];
        std::wstring cur = wa(c->val[i].c_str());
        wcsncpy(t, cur.c_str(), 119); t[119] = 0;
        if (kOpts[i].kind == 'S') swprintf(prompt, 200, L"%ls (numbers separated by commas):", kOpts[i].label);
        else swprintf(prompt, 200, L"%ls (%g to %g):", kOpts[i].label, kOpts[i].lo, kOpts[i].hi);
        if (!dlg_text(md->dlg, L"HLSL Option", prompt, t, 120)) return;
        {
            char *u = w_to_u8(t);
            std::string v = u ? u : "";
            free(u);
            if (!valid_value(kOpts[i], v)) { msg_box(md->dlg, L"HLSL Option", L"This value is not valid for the option.", MB_ICONWARNING, NULL); return; }
            c->val[i] = v;
        }
    }
    fill_row(c, i);
}

static int hlsl_cmd(Modal *md, int id) {
    HCtx *c = (HCtx *)md->user;
    int sel = ListView_GetNextItem(c->lv, -1, LVNI_SELECTED);
    if (id == 1001) { edit_row(md, sel); return 1; }
    if (id == 1002) { if (sel >= 0) { c->val[sel] = kOpts[sel].def; fill_row(c, sel); } return 1; }
    if (id == 1003) { int i; for (i = 0; i < NOPT; i++) { c->val[i] = kOpts[i].def; fill_row(c, i); } return 1; }
    if (id == 1011) {   /* apply the chosen preset */
        int p = (int)SendMessageW(c->cbPre, CB_GETCURSEL, 0, 0), i;
        if (p < 0 || p >= (int)(sizeof kPresets / sizeof kPresets[0])) return 1;
        parse_words(kPresets[p].words, c);
        for (i = 0; i < NOPT; i++) fill_row(c, i);
        return 1;
    }
    return 0;
}

static LRESULT hlsl_notify(Modal *md, NMHDR *nh) {
    if (nh->code == NM_DBLCLK) { HCtx *c = (HCtx *)md->user; edit_row(md, ListView_GetNextItem(c->lv, -1, LVNI_SELECTED)); return 0; }
    if (nh->code == NM_CUSTOMDRAW) return lv_altrows((LPNMLVCUSTOMDRAW)nh);
    return 0;
}

static int hlsl_ok(Modal *md) {
    HCtx *c = (HCtx *)md->user;
    std::string w = words_of(c);
    wchar_t *ww = u8_to_w(w.c_str(), -1);
    int fits = wcslen(ww) < sizeof g_set.hlslOpts / sizeof(wchar_t);
    if (fits) wcscpy(g_set.hlslOpts, ww);
    free(ww);
    if (!fits) { MessageBoxW(md->dlg, L"Too many changed options for the settings file: reset some of them to their defaults.", L"HLSL Settings", MB_OK | MB_ICONINFORMATION); return 0; }
    return 1;
}

void hlsl_dialog(HWND owner) {
    Modal md;
    HCtx c;
    LVCOLUMNW col;
    int i, cw = 560, ch = 570;
    {   /* not taller than the screen (a small screen with a large text size): the list gets the rest */
        RECT wa;
        int avail;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        avail = MulDiv(wa.bottom - wa.top, 96, g_dpi) - 90;
        if (ch > avail) ch = avail > 380 ? avail : 380;
    }
    char *u;
    memset(&md, 0, sizeof md);
    md.kind = 9; md.owner = owner; md.user = &c; md.onok = hlsl_ok; md.oncmd = hlsl_cmd; md.onnotify = hlsl_notify;
    wcscpy(md.title, L"HLSL Settings");
    make_modal(&md, cw, ch);
    u = w_to_u8(g_set.hlslOpts);
    parse_words(u ? u : "", &c);
    free(u);
    mkat(md.dlg, L"STATIC", L"Preset:", SS_LEFT, 10, 14, 60, 18, 0);
    c.cbPre = mkat(md.dlg, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 70, 10, 250, 200, 0);
    for (i = 0; i < (int)(sizeof kPresets / sizeof kPresets[0]); i++) SendMessageW(c.cbPre, CB_ADDSTRING, 0, (LPARAM)kPresets[i].name);
    SendMessageW(c.cbPre, CB_SETCURSEL, 0, 0);
    mkat(md.dlg, L"BUTTON", L"Apply Preset", BS_PUSHBUTTON | WS_TABSTOP, 330, 9, 100, 26, 1011);
    mkat(md.dlg, L"BUTTON", L"Reset All", BS_PUSHBUTTON | WS_TABSTOP, 440, 9, 110, 26, 1003);
    c.lv = mkat(md.dlg, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS | WS_TABSTOP | WS_VSCROLL | WS_BORDER, 10, 44, cw - 20, ch - 44 - 86, 5000);
    ListView_SetExtendedListViewStyle(c.lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    memset(&col, 0, sizeof col);
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"Group"; col.cx = S(110); ListView_InsertColumn(c.lv, 0, &col);
    col.pszText = (LPWSTR)L"Option"; col.cx = S(210); ListView_InsertColumn(c.lv, 1, &col);
    col.pszText = (LPWSTR)L"Value"; col.cx = S(100); ListView_InsertColumn(c.lv, 2, &col);
    col.pszText = (LPWSTR)L"Default"; col.cx = S(90); ListView_InsertColumn(c.lv, 3, &col);
    for (i = 0; i < NOPT; i++) {
        LVITEMW it;
        std::wstring d = wa(kOpts[i].def);
        memset(&it, 0, sizeof it);
        it.mask = LVIF_TEXT; it.iItem = i; it.pszText = (LPWSTR)kOpts[i].group;
        ListView_InsertItem(c.lv, &it);
        ListView_SetItemText(c.lv, i, 1, (LPWSTR)kOpts[i].label);
        if (kOpts[i].kind == 'B') d = d == L"1" ? L"On" : L"Off";
        ListView_SetItemText(c.lv, i, 3, (LPWSTR)d.c_str());
        fill_row(&c, i);
    }
    mkat(md.dlg, L"BUTTON", L"Edit Value…", BS_PUSHBUTTON | WS_TABSTOP, 10, ch - 76, 110, 26, 1001);
    mkat(md.dlg, L"BUTTON", L"Reset Value", BS_PUSHBUTTON | WS_TABSTOP, 130, ch - 76, 110, 26, 1002);
    mkat(md.dlg, L"STATIC", L"Double-click a row to change its value. Only values that differ from MAME's defaults are passed, when HLSL is on.", SS_LEFT, 10, ch - 42, cw - 200, 36, 0);
    mkat(md.dlg, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, cw - 190, ch - 36, 80, 26, IDOK);
    mkat(md.dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, cw - 100, ch - 36, 90, 26, IDCANCEL);
    run_modal(&md);
}
