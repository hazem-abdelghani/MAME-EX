/* The key and pad mapping of the Controls tab. MAME keeps the mapping of its input ports in cfg\default.cfg of its own folder:
       <port type="P1_BUTTON1"><newseq type="standard">KEYCODE_LCONTROL OR JOYCODE_1_BUTTON1</newseq></port>
   This file reads and writes those entries (only the ports listed here; everything else in the file stays as it is). An action that was not
   changed has no entry: MAME's own defaults (keys, and the automatic mapping of game pads) stay in force for it. */
#include "common.h"
#include <mmsystem.h>

#include <string>
#include <vector>
#include "bind_core.inc"

/* ---------------------------------------------------------------- keys: Windows key -> MAME code */
typedef struct { int vk; const char *token; const wchar_t *name; } KeyDef;
static std::vector<KeyDef> g_keys;

static void keys_init(void) {
    static const KeyDef misc[] = {
        {VK_ESCAPE, "ESC", L"Esc"}, {VK_OEM_3, "TILDE", L"` ~"}, {VK_OEM_MINUS, "MINUS", L"-"}, {VK_OEM_PLUS, "EQUALS", L"="}, {VK_BACK, "BACKSPACE", L"Backspace"},
        {VK_TAB, "TAB", L"Tab"}, {VK_OEM_4, "OPENBRACE", L"["}, {VK_OEM_6, "CLOSEBRACE", L"]"}, {VK_RETURN, "ENTER", L"Enter"}, {VK_OEM_1, "COLON", L";"},
        {VK_OEM_7, "QUOTE", L"'"}, {VK_OEM_5, "BACKSLASH", L"\\"}, {VK_OEM_COMMA, "COMMA", L","}, {VK_OEM_PERIOD, "STOP", L"."}, {VK_OEM_2, "SLASH", L"/"},
        {VK_SPACE, "SPACE", L"Space"}, {VK_INSERT, "INSERT", L"Insert"}, {VK_HOME, "HOME", L"Home"}, {VK_END, "END", L"End"}, {VK_PRIOR, "PGUP", L"Page Up"},
        {VK_NEXT, "PGDN", L"Page Down"}, {VK_LEFT, "LEFT", L"Left"}, {VK_RIGHT, "RIGHT", L"Right"}, {VK_UP, "UP", L"Up"}, {VK_DOWN, "DOWN", L"Down"},
        {VK_DIVIDE, "SLASHPAD", L"Num /"}, {VK_MULTIPLY, "ASTERISK", L"Num *"}, {VK_SUBTRACT, "MINUSPAD", L"Num -"}, {VK_ADD, "PLUSPAD", L"Num +"}, {VK_DECIMAL, "DELPAD", L"Num ."},
        {VK_PAUSE, "PAUSE", L"Pause"}, {VK_LSHIFT, "LSHIFT", L"Left Shift"}, {VK_RSHIFT, "RSHIFT", L"Right Shift"}, {VK_LCONTROL, "LCONTROL", L"Left Ctrl"},
        {VK_RCONTROL, "RCONTROL", L"Right Ctrl"}, {VK_LMENU, "LALT", L"Left Alt"}, {VK_RMENU, "RALT", L"Right Alt"}, {VK_SCROLL, "SCRLOCK", L"Scroll Lock"},
        {VK_NUMLOCK, "NUMLOCK", L"Num Lock"}, {VK_CAPITAL, "CAPSLOCK", L"Caps Lock"}, {VK_SNAPSHOT, "PRTSCR", L"Print Screen"}, {VK_LWIN, "LWIN", L"Left Windows"},
        {VK_RWIN, "RWIN", L"Right Windows"}, {VK_APPS, "MENU", L"Menu"}};
    int i;
    static char names[64][8];
    static wchar_t wnames[64][12];
    int nn = 0;
    if (!g_keys.empty()) return;
    for (i = 'A'; i <= 'Z'; i++) { snprintf(names[nn], 8, "%c", i); swprintf(wnames[nn], 12, L"%c", i); g_keys.push_back({i, names[nn], wnames[nn]}); nn++; }
    for (i = '0'; i <= '9'; i++) { snprintf(names[nn], 8, "%c", i); swprintf(wnames[nn], 12, L"%c", i); g_keys.push_back({i, names[nn], wnames[nn]}); nn++; }
    for (i = 1; i <= 12; i++) { snprintf(names[nn], 8, "F%d", i); swprintf(wnames[nn], 12, L"F%d", i); g_keys.push_back({VK_F1 + i - 1, names[nn], wnames[nn]}); nn++; }
    for (i = 0; i <= 9; i++) { snprintf(names[nn], 8, "%dPAD", i); swprintf(wnames[nn], 12, L"Num %d", i); g_keys.push_back({VK_NUMPAD0 + i, names[nn], wnames[nn]}); nn++; }
    for (i = 0; i < (int)(sizeof misc / sizeof misc[0]); i++) g_keys.push_back(misc[i]);
}

static const wchar_t *key_name(const char *token) {
    size_t i;
    keys_init();
    for (i = 0; i < g_keys.size(); i++) if (!strcmp(g_keys[i].token, token)) return g_keys[i].name;
    return NULL;
}

/* "1" -> "Pad 1 Button 1", "1_HAT1UP" -> "Pad 1 D-Pad Up" ... */
static void pad_name(const char *code, wchar_t *out, size_t cap) {
    int n = 0, used = 0;
    const char *r;
    wchar_t what[80];
    if (sscanf(code, "%d_%n", &n, &used) < 1 || !used) { wchar_t *w = u8_to_w(code, -1); _snwprintf(out, cap, L"%ls", w); free(w); out[cap - 1] = 0; return; }
    r = code + used;
    {   /* the name MAME itself gives the control (from the probe run of MAME): A, LT, D-pad Up ... */
        char item[48], nm[48];
        size_t l = strcspn(r, "_");
        if (l && l < sizeof item && (r[l] == 0 || strstr(r, "_NEG_") || strstr(r, "_POS_"))) {
            memcpy(item, r, l); item[l] = 0;
            if (padprobe_itemname(n, item, nm, sizeof nm)) {
                wchar_t *w = u8_to_w(nm, -1);
                _snwprintf(out, cap, L"Pad %d %ls", n, w);
                free(w);
                out[cap - 1] = 0;
                return;
            }
        }
    }
    if (!strncmp(r, "BUTTON", 6)) {   /* the names of an Xbox pad (what MAME numbers 1-8 for XInput); other buttons keep their number */
        static const wchar_t *xb[] = {L"A", L"B", L"X", L"Y", L"LB", L"RB", L"Left Stick Click", L"Right Stick Click"};
        int k = atoi(r + 6);
        if (k >= 1 && k <= 8) wcscpy(what, xb[k - 1]); else swprintf(what, 80, L"Button %d", k);
    }
    else if (!strcmp(r, "HAT1UP")) wcscpy(what, L"D-Pad Up");
    else if (!strcmp(r, "HAT1DOWN")) wcscpy(what, L"D-Pad Down");
    else if (!strcmp(r, "HAT1LEFT")) wcscpy(what, L"D-Pad Left");
    else if (!strcmp(r, "HAT1RIGHT")) wcscpy(what, L"D-Pad Right");
    else if (!strcmp(r, "XAXIS_LEFT_SWITCH")) wcscpy(what, L"Stick Left");
    else if (!strcmp(r, "XAXIS_RIGHT_SWITCH")) wcscpy(what, L"Stick Right");
    else if (!strcmp(r, "YAXIS_UP_SWITCH")) wcscpy(what, L"Stick Up");
    else if (!strcmp(r, "YAXIS_DOWN_SWITCH")) wcscpy(what, L"Stick Down");
    else if (!strcmp(r, "SLIDER1_NEG_SWITCH")) wcscpy(what, L"LT");
    else if (!strcmp(r, "SLIDER2_NEG_SWITCH")) wcscpy(what, L"RT");
    else if (!strcmp(r, "START")) wcscpy(what, L"Start");
    else if (!strcmp(r, "SELECT")) wcscpy(what, L"Back / Select");
    else { wchar_t *w = u8_to_w(r, -1); _snwprintf(what, 80, L"%ls", w); what[79] = 0; free(w); }
    _snwprintf(out, cap, L"Pad %d %ls", n, what);
    out[cap - 1] = 0;
}

void key_display(const char *token, wchar_t *out, size_t cap) {
    const wchar_t *kn;
    keys_init();
    kn = token[0] ? key_name(token) : NULL;
    if (token[0] && !kn) { wchar_t *w = u8_to_w(token, -1); _snwprintf(out, cap, L"%ls", w); free(w); }
    else _snwprintf(out, cap, L"%ls", kn ? kn : L"-");
    out[cap - 1] = 0;
}
void pad_display(const char *code, wchar_t *out, size_t cap) {
    if (code[0]) pad_name(code, out, cap); else _snwprintf(out, cap, L"-");
    out[cap - 1] = 0;
}

void bind_text(int row, wchar_t *key, wchar_t *pad, size_t cap) {
    const Bind *b = &g_b[row];
    const wchar_t *kn;
    key[0] = pad[0] = 0;
    if (!b->custom) {   /* not changed: what MAME does by itself */
        kn = kRows[row].defKey[0] ? key_name(kRows[row].defKey) : NULL;
        _snwprintf(key, cap, L"%ls", kn ? kn : L"-");
        _snwprintf(pad, cap, L"MAME default");
        key[cap - 1] = pad[cap - 1] = 0;
        return;
    }
    kn = b->key[0] ? key_name(b->key) : NULL;
    if (b->key[0] && !kn) { wchar_t *w = u8_to_w(b->key, -1); _snwprintf(key, cap, L"%ls", w); free(w); }
    else _snwprintf(key, cap, L"%ls", kn ? kn : L"-");
    {   /* all the pad codes: "Pad 1 D-Pad Up / Pad 1 Stick Up" */
        std::vector<std::string> codes;
        size_t i;
        bind_pad_list(b, codes);
        pad[0] = 0;
        if (codes.empty()) _snwprintf(pad, cap, L"-");
        for (i = 0; i < codes.size(); i++) {
            wchar_t one[100];
            pad_name(codes[i].c_str(), one, 100);
            if (i) wcsncat(pad, L" / ", cap - wcslen(pad) - 1);
            wcsncat(pad, one, cap - wcslen(pad) - 1);
        }
    }
    key[cap - 1] = pad[cap - 1] = 0;
}

/* ---------------------------------------------------------------- capture: a key */
typedef struct { BOOL was[256]; int row, changed; char out[48]; } KeyCtx;

static void key_timer(Modal *md) {
    KeyCtx *c = (KeyCtx *)md->user;
    size_t i;
    keys_init();
    for (i = 0; i < g_keys.size(); i++) {
        int vk = g_keys[i].vk;
        BOOL d = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (vk == VK_ESCAPE || vk == VK_DELETE) continue;   /* handled below: Esc cancels, Delete unbinds */
        if (d && !c->was[vk]) {
            strncpy(c->out, g_keys[i].token, 47);
            c->changed = 1;
            md->done = 1;
            return;
        }
        c->was[vk] = d;
    }
    {   /* Esc cancels (it is not a key that can be bound here), Delete unbinds */
        BOOL esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0, del = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
        if (esc && !c->was[VK_ESCAPE]) { md->done = 1; return; }
        if (del && !c->was[VK_DELETE]) { c->out[0] = 0; c->changed = 1; md->done = 1; return; }
        c->was[VK_ESCAPE] = esc; c->was[VK_DELETE] = del;
    }
}

int capture_key_token(HWND owner, const wchar_t *label, char *out48, int *cleared) {
    Modal md;
    KeyCtx c;
    wchar_t text[300];
    int vk;
    memset(&md, 0, sizeof md);
    memset(&c, 0, sizeof c);
    md.kind = 8; md.owner = owner; md.user = &c; md.ontimer = key_timer;
    wcscpy(md.title, L"Set Keyboard Key");
    make_modal(&md, 330, 150);
    swprintf(text, 300, L"%ls\n\nPress the key you want to use…\n(Esc = cancel, Delete = no key)", label);
    mkat(md.dlg, L"STATIC", text, SS_CENTER, 10, 10, 310, 85, 0);
    mkat(md.dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 115, 110, 100, 26, IDCANCEL);
    for (vk = 8; vk < 255; vk++) c.was[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;   /* what is held now does not count */
    SetTimer(md.dlg, 1, 8, NULL);
    run_modal(&md);
    if (!c.changed) return 0;
    strcpy(out48, c.out);
    return 1;
}

int bind_capture_key(HWND owner, int row) {
    char k[48];
    int cl = 0;
    if (!capture_key_token(owner, kRows[row].label, k, &cl)) return 0;
    bind_set_key(row, k);
    return 1;
}

/* ---------------------------------------------------------------- capture: a pad button */
typedef struct { WORD wButtons; BYTE bLT, bRT; SHORT sLX, sLY, sRX, sRY; } XPad;
typedef struct { DWORD packet; XPad pad; } XState;
typedef DWORD (WINAPI *PFN_XGET)(DWORD, XState *);

typedef struct {
    HMODULE xi;
    PFN_XGET xget;
    WORD prevX[4];
    BYTE prevT[4][2];   /* triggers (LT, RT) held at the last look */
    DWORD prevJ[16];
    int dir, row, changed, startTick;
    int devno[4];   /* the MAME device number (JOYCODE_n) of each XInput pad, 0 = unknown */
    HWND cbCtl, lbl;
    char out[80];
} PadCtx;

#define XB_UP 0x0001
#define XB_DOWN 0x0002
#define XB_LEFT 0x0004
#define XB_RIGHT 0x0008
#define XB_START 0x0010
#define XB_BACK 0x0020
#define XB_LSB 0x0040
#define XB_RSB 0x0080
#define XB_LB 0x0100
#define XB_RB 0x0200
#define XB_A 0x1000
#define XB_B 0x2000
#define XB_X 0x4000
#define XB_Y 0x8000

/* the state of every pad as a bit set: directions (0x100000 up, 0x200000 down, 0x400000 left, 0x800000 right) and buttons (the low bits) */
enum { D_UP = 1 << 20, D_DOWN = 1 << 21, D_LEFT = 1 << 22, D_RIGHT = 1 << 23 };

static void pad_done(Modal *md, PadCtx *c, int dev, const char *what) {
    int sel = (int)SendMessageW(c->cbCtl, CB_GETCURSEL, 0, 0);
    int n = sel > 0 ? sel : dev;   /* the choice of the box, else the pad that was pressed */
    snprintf(c->out, sizeof c->out, "%d_%s", n, what);
    c->changed = 1;
    md->done = 1;
}

/* an XInput control as MAME's code: the token MAME itself uses for the item of this name (from the probe run), else the usual one */
static void xi_done(Modal *md, PadCtx *c, int user, const char *mameName, const char *fallback, int trigger) {
    char tok[48], what[80];
    int cur = 0, dev = c->devno[user] ? c->devno[user] : user + 1;
    if (padprobe_token(dev, mameName, tok, sizeof tok, &cur)) {
        if (trigger) snprintf(what, sizeof what, "%s_%s_SWITCH", tok, cur <= -16000 ? "POS" : "NEG");
        else snprintf(what, sizeof what, "%s", tok);
    } else snprintf(what, sizeof what, "%s", fallback);
    pad_done(md, c, dev, what);
}

static const char *dir_name(int bit, int stick) {
    if (stick) return bit == D_UP ? "YAXIS_UP_SWITCH" : bit == D_DOWN ? "YAXIS_DOWN_SWITCH" : bit == D_LEFT ? "XAXIS_LEFT_SWITCH" : "XAXIS_RIGHT_SWITCH";
    return bit == D_UP ? "HAT1UP" : bit == D_DOWN ? "HAT1DOWN" : bit == D_LEFT ? "HAT1LEFT" : "HAT1RIGHT";
}

static void pad_timer(Modal *md) {
    PadCtx *c = (PadCtx *)md->user;
    int i;
    static const int dirs[4] = {D_UP, D_DOWN, D_LEFT, D_RIGHT};
    if (c->xget) {
        for (i = 0; i < 4; i++) {
            XState st;
            WORD now, newly;
            if (c->xget((DWORD)i, &st) != 0) { c->prevX[i] = 0; continue; }
            now = st.pad.wButtons;
            newly = (WORD)(now & ~c->prevX[i]);
            c->prevX[i] = now;
            if (c->dir) {
                int d, sx = st.pad.sLX, sy = st.pad.sLY;
                static const WORD dm[4] = {XB_UP, XB_DOWN, XB_LEFT, XB_RIGHT};
                static const char *dn[4] = {"D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"};
                for (d = 0; d < 4; d++) if (newly & dm[d]) { xi_done(md, c, i, dn[d], dir_name(dirs[d], 0), 0); return; }
                if (sy > 16000) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_UP, 1)); return; }
                if (sy < -16000) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_DOWN, 1)); return; }
                if (sx < -16000) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_LEFT, 1)); return; }
                if (sx > 16000) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_RIGHT, 1)); return; }
            } else {
                static const struct { WORD m; const char *n, *t; } bt[] = {{XB_A, "A", "BUTTON1"}, {XB_B, "B", "BUTTON2"}, {XB_X, "X", "BUTTON3"}, {XB_Y, "Y", "BUTTON4"}, {XB_LB, "LB", "BUTTON5"}, {XB_RB, "RB", "BUTTON6"},
                                                                          {XB_LSB, "LSB", "BUTTON7"}, {XB_RSB, "RSB", "BUTTON8"}, {XB_START, "Start", "START"}, {XB_BACK, "Back", "SELECT"}};
                size_t k;
                for (k = 0; k < sizeof bt / sizeof bt[0]; k++) if (newly & bt[k].m) { xi_done(md, c, i, bt[k].n, bt[k].t, 0); return; }
                {   /* the triggers are axes of the pad in MAME: LT = SLIDER1, RT = SLIDER2, on the negative side (the right stick is ZAXIS / RZAXIS) */
                    BYTE lt = st.pad.bLT >= 128, rt = st.pad.bRT >= 128;
                    BYTE wasL = c->prevT[i][0], wasR = c->prevT[i][1];
                    c->prevT[i][0] = lt; c->prevT[i][1] = rt;
                    if (lt && !wasL) { xi_done(md, c, i, "LT", "SLIDER1_NEG_SWITCH", 1); return; }
                    if (rt && !wasR) { xi_done(md, c, i, "RT", "SLIDER2_NEG_SWITCH", 1); return; }
                }
            }
        }
    }
    for (i = 0; i < 16; i++) {   /* the other pads (DirectInput, arcade sticks): through the Windows joystick API */
        JOYINFOEX ji;
        DWORD now = 0, newly;
        memset(&ji, 0, sizeof ji);
        ji.dwSize = sizeof ji;
        ji.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx((UINT)i, &ji) != JOYERR_NOERROR) { c->prevJ[i] = 0; continue; }
        now = ji.dwButtons & 0xFFFFF;
        if (ji.dwPOV != JOY_POVCENTERED && ji.dwPOV <= 36000) {
            DWORD a = ji.dwPOV;
            if (a >= 31500 || a <= 4500) now |= D_UP;
            if (a >= 13500 && a <= 22500) now |= D_DOWN;
            if (a >= 22500 && a <= 31500) now |= D_LEFT;
            if (a >= 4500 && a <= 13500) now |= D_RIGHT;
        }
        {
            int sx = (int)ji.dwXpos - 32768, sy = (int)ji.dwYpos - 32768;
            if (sy < -16000) now |= 1 << 24;   /* stick up */
            if (sy > 16000) now |= 1 << 25;
            if (sx < -16000) now |= 1 << 26;
            if (sx > 16000) now |= 1 << 27;
        }
        newly = now & ~c->prevJ[i];
        c->prevJ[i] = now;
        if (!newly) continue;
        if (c->dir) {
            int d;
            for (d = 0; d < 4; d++) if (newly & dirs[d]) { pad_done(md, c, i + 1, dir_name(dirs[d], 0)); return; }
            if (newly & (1 << 24)) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_UP, 1)); return; }
            if (newly & (1 << 25)) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_DOWN, 1)); return; }
            if (newly & (1 << 26)) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_LEFT, 1)); return; }
            if (newly & (1 << 27)) { pad_done(md, c, c->devno[i] ? c->devno[i] : i + 1, dir_name(D_RIGHT, 1)); return; }
        } else {
            int k;
            char what[24];
            for (k = 0; k < 20; k++) if (newly & (1u << k)) { snprintf(what, sizeof what, "BUTTON%d", k + 1); pad_done(md, c, i + 1, what); return; }
        }
    }
}

static int pad_cmd(Modal *md, int id) {
    PadCtx *c = (PadCtx *)md->user;
    if (id == 1001) { c->out[0] = 0; c->changed = 1; md->done = 1; return 1; }   /* No Pad Button */
    return 0;
}

int capture_pad_token(HWND owner, const wchar_t *label, int dir, char *out72) {
    Modal md;
    PadCtx c;
    wchar_t text[300];
    int i;
    memset(&md, 0, sizeof md);
    memset(&c, 0, sizeof c);
    c.dir = dir;
    c.xi = LoadLibraryW(L"xinput1_4.dll");
    if (!c.xi) c.xi = LoadLibraryW(L"xinput1_3.dll");
if (c.xi) c.xget = (PFN_XGET)GetProcAddress(c.xi, "XInputGetState");
    if (c.xget) {   /* what MAME calls the controls of the pads (a short MAME run, once) */
        padprobe_run(0);
        for (i = 0; i < 4; i++) c.devno[i] = padprobe_devno(i + 1);
    }
    md.kind = 8; md.owner = owner; md.user = &c; md.ontimer = pad_timer; md.oncmd = pad_cmd;
    wcscpy(md.title, L"Set Pad Button");
    make_modal(&md, 360, 190);
    swprintf(text, 300, c.dir ? L"%ls\n\nMove the D-pad or the stick of the pad you want to use…" : L"%ls\n\nPress the button you want to use…", label);
    mkat(md.dlg, L"STATIC", text, SS_CENTER, 10, 10, 340, 60, 0);
    mkat(md.dlg, L"STATIC", L"Pad Number:", SS_LEFT, 40, 84, 90, 18, 0);
    c.cbCtl = mkat(md.dlg, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 135, 80, 185, 200, 0);
    SendMessageW(c.cbCtl, CB_ADDSTRING, 0, (LPARAM)L"Automatic (The Pad Pressed)");
    for (i = 1; i <= 8; i++) { wchar_t t[24]; swprintf(t, 24, L"Pad %d", i); SendMessageW(c.cbCtl, CB_ADDSTRING, 0, (LPARAM)t); }
    SendMessageW(c.cbCtl, CB_SETCURSEL, 0, 0);
    mkat(md.dlg, L"BUTTON", L"No Pad Button", BS_PUSHBUTTON | WS_TABSTOP, 40, 140, 120, 26, 1001);
    mkat(md.dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 220, 140, 100, 26, IDCANCEL);
    /* what is held now does not count */
    if (c.xget) for (i = 0; i < 4; i++) { XState st; if (c.xget((DWORD)i, &st) == 0) { c.prevX[i] = st.pad.wButtons; c.prevT[i][0] = st.pad.bLT >= 128; c.prevT[i][1] = st.pad.bRT >= 128; } }
    for (i = 0; i < 16; i++) {
        JOYINFOEX ji;
        memset(&ji, 0, sizeof ji);
        ji.dwSize = sizeof ji; ji.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx((UINT)i, &ji) == JOYERR_NOERROR) {
            DWORD now = ji.dwButtons & 0xFFFFF;
            int sx = (int)ji.dwXpos - 32768, sy = (int)ji.dwYpos - 32768;
            if (ji.dwPOV != JOY_POVCENTERED && ji.dwPOV <= 36000) {
                DWORD a = ji.dwPOV;
                if (a >= 31500 || a <= 4500) now |= D_UP;
                if (a >= 13500 && a <= 22500) now |= D_DOWN;
                if (a >= 22500 && a <= 31500) now |= D_LEFT;
                if (a >= 4500 && a <= 13500) now |= D_RIGHT;
            }
            if (sy < -16000) now |= 1 << 24;
            if (sy > 16000) now |= 1 << 25;
            if (sx < -16000) now |= 1 << 26;
            if (sx > 16000) now |= 1 << 27;
            c.prevJ[i] = now;
        }
    }
    SetTimer(md.dlg, 1, 15, NULL);
    run_modal(&md);
    if (c.xi) FreeLibrary(c.xi);
    if (!c.changed) return 0;
    strncpy(out72, c.out, 71); out72[71] = 0;
    return 1;
}

int bind_capture_pad(HWND owner, int row) {
    char p[72];
    if (!capture_pad_token(owner, kRows[row].label, kRows[row].dir, p)) return 0;
    bind_set_pad(row, p);
    return 1;
}

/* the pads of this PC as MAME numbers them (the probe run), else player n = JOYCODE_n */
int bind_pad_dev(int pl) { int d = padprobe_devno(pl); return d ? d : pl; }
int bind_pad_xinput(int dev) { return padprobe_is_xinput(dev); }
std::string bind_trigger_code(int dev, int right) {
    char tok[48], out[96];
    int cur = 0;
    if (padprobe_token(dev, right ? "RT" : "LT", tok, sizeof tok, &cur)) { snprintf(out, sizeof out, "%d_%s_%s_SWITCH", dev, tok, cur <= -16000 ? "POS" : "NEG"); return out; }
    if (!padprobe_is_xinput(dev)) { snprintf(out, sizeof out, "%d_ZAXIS_%s_SWITCH", dev, right ? "NEG" : "POS"); return out; }   /* a DirectInput Xbox pad: LT and RT share the Z axis */
    snprintf(out, sizeof out, "%d_SLIDER%d_NEG_SWITCH", dev, right ? 2 : 1);
    return out;
}

#include "bind_migrate.inc"
