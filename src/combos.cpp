/* The "Combos" tab: triggers (a key or a pad button) that make MAME play a move sequence for a player.
   The sequences are shown and typed in words ("Quarter-circle Forward + Heavy Punch", "Down, Down-Right, Right + Heavy Punch");
   mame-ex-combos.cfg keeps the short numpad notation (236+HP: directions 1-9 as on a numpad, buttons LP MP HP LK MK HK,
   "+" together, "~" a pause, "4*C" hold for the charge time). Typing the short notation also works.
   When a combo has a trigger and a sequence the launcher writes a small MAME Lua plugin (mame-ex-plugins\mame-ex-combos-plugin\mamex_combos) and starts MAME with it:
   the plugin watches the triggers and presses the buttons of the player for the steps of the sequence. */
#include "common.h"
#include <string>

#define NUM_COMBOS 20
typedef struct {
    char name[160], kind[40], seq[160];
    int player;              /* 1 or 2 */
    int faceLeft;
    char key[48], pad[72];   /* trigger: the keyboard key / pad button, like the Controls tab */
} ComboSlot;
static struct { ComboSlot slots[NUM_COMBOS]; int stepMs, chargeMs; } g_in;

typedef struct {
    HWND page, lStep, eStep, lChg, eChg, lLoad, cbLoad, lv, lChar, cbChar, lMove, cbMove, lSeq, eSeq, lPlay, cbPlay, lFace, cbFace, bClear, bLegend, bHow, bDef, note, tok[CMB_NTOK], intro;
    int syncing, loading;
} CmbUI;
static CmbUI g_cmb;
void cmb_refresh(void);
static void fill_form(void);
void cmb_collect(void);

static const wchar_t *INFO =
    L"HOW COMBOS WORK\n"
    L"Give a row a trigger (click its Keyboard or Pad cell) and the move of that row is played for you whenever you press the trigger in a game. Double-click a name to rename a combo.\n\n"
    L"FILL A ROW\n"
    L"\u2022 Load Character puts every move of a character into the rows. Or pick a Character and a Move below to set the selected row.\n"
    L"\u2022 Or write your own Sequence: type it or use the buttons under the Sequence box. Steps are separated by commas, + presses buttons together.\n"
    L"    Example: D, DR, R + HP   (the same as QCF + HP)\n"
    L"\u2022 The short names (QCF, HP, DR, BN1 ...) are explained by the Legend button. The long names (Quarter-circle Forward, Heavy Punch) work as well. ~ is a short wait (one step, 30 ms by default).\n\n"
    L"CHARGE MOVES (Guile, Blanka, Balrog, Vega, Cracker Jack)\n"
    L"\"Hold L\" holds that direction for the Charge time (default 600 ms). If a charge move does not come out, raise the Charge time a little: the game needs its own minimum of charge.\n\n"
    L"GOOD TO KNOW\n"
    L"\u2022 Player Side: Left (default) = your character is on the left of the screen and looks right. Right = it is on the right and looks left; Left / Right then swap in the sequence.\n"
    L"\u2022 Supers and Meteor Combos need super gauge.\n"
    L"\u2022 The short number notation (236+HP) still works.\n\n"
    L"Note: the move lists (Load Character) are from Street Fighter EX2 Plus. Combos are made for the Street Fighter games; other games can use them too, but the characters, moves and the button names are Street Fighter's.";

/* the Legend button: what the short names stand for, in a small table */
static void dlg_legend(void) {
    static const struct { const wchar_t *a, *b; } ROWS[] = {
        {L"DIRECTIONS", NULL}, {L"", L"for Player Side Left (the character looks right); Left and Right swap when Player Side is Right"},
        {L"U", L"Up"}, {L"D", L"Down"}, {L"L", L"Left"}, {L"R", L"Right"},
        {L"UL", L"Up-Left"}, {L"UR", L"Up-Right"}, {L"DL", L"Down-Left"}, {L"DR", L"Down-Right"},
        {L"F", L"Forward (towards the opponent)"}, {L"B", L"Back (away from the opponent)"}, {L"N", L"Neutral"},
        {L"BUTTONS", NULL},
        {L"LP", L"Light Punch (Button 1)"}, {L"MP", L"Medium Punch (Button 2)"}, {L"HP", L"Heavy Punch (Button 3)"},
        {L"LK", L"Light Kick (Button 4)"}, {L"MK", L"Medium Kick (Button 5)"}, {L"HK", L"Heavy Kick (Button 6)"},
        {L"BN1 ... BN6", L"Button 1 ... 6 of the Controls tab (the same buttons as LP ... HK)"}, {L"START", L"The Start button"},
        {L"MOTION PIECES", NULL},
        {L"Q", L"Quarter"}, {L"H", L"Half"}, {L"C", L"Circle"}, {L"D (in front)", L"Double (twice)"},
        {L"CF", L"Circle Forward"}, {L"CB", L"Circle Back"}, {L"R (in front)", L"Reverse"},
        {L"DP", L"Dragon Punch motion"}, {L"FCF / FCB", L"Full Circle Forward / Back"},
        {L"MOTIONS", NULL},
        {L"QCF", L"Quarter-circle Forward  (D, DR, R)"}, {L"QCB", L"Quarter-circle Back  (D, DL, L)"},
        {L"HCF", L"Half-circle Forward  (L, DL, D, DR, R)"}, {L"HCB", L"Half-circle Back  (R, DR, D, DL, L)"},
        {L"DP  or  FDP", L"Dragon-punch Motion, forward  (R, D, DR)"}, {L"RDP  or  BDP", L"Reverse Dragon-punch Motion, back  (L, D, DL)"},
        {L"FCF", L"Full Circle Forward  (R, DR, D, DL, L, UL)"}, {L"FCB", L"Full Circle Back  (L, DL, D, DR, R, UR)"},
        {L"DFC", L"Double Full Circle  (two turns, forward)"},
        {L"DQCF", L"Double Quarter-circle Forward  (QCF twice)"}, {L"DQCB", L"Double Quarter-circle Back  (QCB twice)"},
        {L"SIGNS", NULL},
        {L"+", L"Pressed together (QCF + HP)"}, {L",", L"The next step"},
        {L"~", L"A short wait: one neutral step as long as Step (ms), 30 ms by default (~ ~ = 60 ms)"},
        {L"Hold L", L"Hold that direction for the Charge time (default 600 ms), for charge moves"}};
    dlg_table(ui_owner(g_main), L"Legend", L"Short Name", L"Meaning", (const TableRow *)ROWS, (int)(sizeof ROWS / sizeof ROWS[0]), 100, 520);
}

/* the buttons under the Sequence box: the label is what they write (+ , and Del are the exceptions) */
static const char *TOKS[CMB_NTOK] = {   /* 3 rows of 10 cells, NULL = empty cell */
    "UL", "U", "UR", "LP", "MP", "HP", "HCB", "HCF", "~", "Del",
    "L", NULL, "R", "LK", "MK", "HK", "QCB", "QCF", "+", "START",
    "DL", "D", "DR", NULL, NULL, NULL, "FCB", "FCF", ",", "Hold"};

/* a button picture from the PNG files in icons (via icon_data.c), scaled to the screen DPI; NULL when the token has no picture */
static HICON token_icon(const char *name) {
    int k, w, h, nw, nh, x, y;
    const IconData *ic = NULL;
    BITMAPINFO bi;
    void *bits = NULL;
    HBITMAP color, mask;
    ICONINFO ii;
    HICON ico;
    unsigned char *dst;
    for (k = 0; k < g_nIcons; k++) if (!strcmp(g_icons[k].name, name)) ic = &g_icons[k];
    if (!ic) return NULL;
    w = ic->w; h = ic->h;
    nw = S(w); nh = S(h);
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = nw; bi.bmiHeader.biHeight = -nh; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    color = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!color) return NULL;
    dst = (unsigned char *)bits;
    for (y = 0; y < nh; y++) for (x = 0; x < nw; x++) {
        /* bilinear on premultiplied colours, then back to straight alpha (the format of icons) */
        double fx = nw > 1 ? (double)x * (w - 1) / (nw - 1) : 0, fy = nh > 1 ? (double)y * (h - 1) / (nh - 1) : 0, acc[4] = {0, 0, 0, 0};
        int x0 = (int)fx, y0 = (int)fy, x1 = x0 + 1 < w ? x0 + 1 : x0, y1 = y0 + 1 < h ? y0 + 1 : y0, c;
        double tx = fx - x0, ty = fy - y0, wgt[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
        int px[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}}, q;
        for (q = 0; q < 4; q++) {
            const unsigned char *p = ic->bgra + ((size_t)px[q][1] * w + px[q][0]) * 4;
            double a = p[3] / 255.0;
            acc[0] += wgt[q] * p[0] * a; acc[1] += wgt[q] * p[1] * a; acc[2] += wgt[q] * p[2] * a; acc[3] += wgt[q] * a;
        }
        for (c = 0; c < 3; c++) dst[((size_t)y * nw + x) * 4 + c] = acc[3] > 0.0001 ? (unsigned char)(acc[c] / acc[3] + 0.5) : 0;
        dst[((size_t)y * nw + x) * 4 + 3] = (unsigned char)(acc[3] * 255 + 0.5);
    }
    mask = CreateBitmap(nw, nh, 1, 1, NULL);
    memset(&ii, 0, sizeof ii);
    ii.fIcon = TRUE; ii.hbmColor = color; ii.hbmMask = mask;
    ico = CreateIconIndirect(&ii);
    DeleteObject(color); DeleteObject(mask);
    return ico;
}

static int g_rows;   /* rows shown in the list */

/* ---------------------------------------------------------------- sequences in words */
static const char *DIRW[10] = {"", "DL", "D", "DR", "L", "N", "R", "UL", "U", "UR"};   /* shown: D = Down, DR = Down-Right, ... */
static const char *DIRFULL[10] = {"", "Down-Left", "Down", "Down-Right", "Left", "Neutral", "Right", "Up-Left", "Up", "Up-Right"};   /* also accepted when typing */
static const char *BTNW[6][2] = {{"LP", "Light Punch"}, {"MP", "Medium Punch"}, {"HP", "Heavy Punch"}, {"LK", "Light Kick"}, {"MK", "Medium Kick"}, {"HK", "Heavy Kick"}};
static const char *MOTION[][3] = {   /* numpad digits, full name, short name */
    {"236", "Quarter-circle Forward", "QCF"}, {"214", "Quarter-circle Back", "QCB"}, {"623", "Dragon-punch Motion", "DP"}, {"421", "Reverse Dragon-punch Motion", "RDP"},
    {"41236", "Half-circle Forward", "HCF"}, {"63214", "Half-circle Back", "HCB"}, {"632147", "Full Circle Forward", "FCF"}, {"412369", "Full Circle Back", "FCB"}, {"632147632147", "Double Full Circle", "DFC"},
    {"236236", "Double Quarter-circle Forward", "DQCF"}, {"214214", "Double Quarter-circle Back", "DQCB"}};

static int mirror(int d) { return d == 1 ? 3 : d == 3 ? 1 : d == 4 ? 6 : d == 6 ? 4 : d == 7 ? 9 : d == 9 ? 7 : d; }
static void app(char *o, int cap, const char *s) { size_t l = strlen(o); if ((int)l < cap - 1) snprintf(o + l, (size_t)cap - l, "%s", s); }

/* is this the short notation the plugin reads (and not text that is still being typed)? */
static int is_notation(const char *seq) {
    char buf[512], *tok, *ctx = 0;
    strncpy(buf, seq, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (tok = strtok_s(buf, ", \t", &ctx); tok; tok = strtok_s(NULL, ", \t", &ctx)) {
        const char *p = tok;
        if (!strcmp(tok, "~")) continue;
        while (*p >= '1' && *p <= '9') p++;
        if (p > tok && *p == '*') { p++; if (*p == 'C' || *p == 'c') p++; else while (*p >= '0' && *p <= '9') p++; }
        while (*p) {
            const char *e;
            char nm[8];
            size_t n;
            int ok = 0, k;
            static const char *NAMES[] = {"LP", "MP", "HP", "LK", "MK", "HK", "ST", "START", "B1", "B2", "B3", "B4", "B5", "B6", "BN1", "BN2", "BN3", "BN4", "BN5", "BN6", NULL};
            if (*p == '+') p++;
            e = p; while (*e && *e != '+') e++;
            n = (size_t)(e - p);
            if (n == 0 || n > 5) return 0;
            memcpy(nm, p, n); nm[n] = 0;
            for (k = 0; NAMES[k]; k++) if (!_stricmp(NAMES[k], nm)) ok = 1;
            if (!ok) return 0;
            p = e;
        }
    }
    return 1;
}

/* numpad notation -> words ("236+HP" -> "Quarter-circle Forward (Down, Down-Right, Right) + Heavy Punch") */
static void seq_to_words(const char *seq, int left, char *out, int cap) {
    char buf[512], *tok, *ctx = 0;
    out[0] = 0;
    if (!is_notation(seq)) { strncpy(out, seq, (size_t)cap - 1); out[cap - 1] = 0; return; }   /* text that is still being typed: shown as typed */
    strncpy(buf, seq, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (tok = strtok_s(buf, ", \t", &ctx); tok; tok = strtok_s(NULL, ", \t", &ctx)) {
        char step[300] = "", digits[40];
        const char *p = tok;
        int nd = 0, i, held = 0, ms = 0;
        if (out[0]) app(out, cap, ", ");
        if (!strcmp(tok, "~")) { app(out, cap, "~"); continue; }
        while (*p >= '1' && *p <= '9' && nd < 39) digits[nd++] = *p++;
        digits[nd] = 0;
        if (nd > 0 && *p == '*') {
            p++;
            if (*p == 'C' || *p == 'c') { held = 1; ms = -1; p++; }
            else { ms = atoi(p); held = 1; while (*p >= '0' && *p <= '9') p++; }
        }
        if (nd > 0) {
            int motion = -1;
            if (!held) for (i = 0; i < (int)(sizeof MOTION / sizeof MOTION[0]); i++) if (!strcmp(MOTION[i][0], digits)) motion = i;
            if (motion >= 0) app(step, sizeof step, MOTION[motion][2]), app(step, sizeof step, " (");
            for (i = 0; i < nd - (held ? 1 : 0); i++) { if (i) app(step, sizeof step, ", "); app(step, sizeof step, DIRW[left ? mirror(digits[i] - '0') : digits[i] - '0']); }
            if (held) {
                char h[60];
                if (nd > 1) app(step, sizeof step, ", ");
                snprintf(h, sizeof h, ms < 0 ? "Hold %s (charge)" : "Hold %s (%d ms)", DIRW[left ? mirror(digits[nd - 1] - '0') : digits[nd - 1] - '0'], ms);
                app(step, sizeof step, h);
            }
            if (motion >= 0) app(step, sizeof step, ")");
        }
        while (*p) {
            const char *e;
            char nm[12];
            size_t n;
            int k, found = -1;
            if (*p == '+') p++;
            e = p; while (*e && *e != '+') e++;
            n = (size_t)(e - p);
            if (n == 0) break;
            if (n > 11) n = 11;
            memcpy(nm, p, n); nm[n] = 0;
            for (k = 0; k < 6; k++) if (!_stricmp(BTNW[k][0], nm)) found = k;
            if (found < 0 && (nm[0] == 'B' || nm[0] == 'b') && nm[1] >= '1' && nm[1] <= '6' && !nm[2]) found = nm[1] - '1';
            if (step[0]) app(step, sizeof step, " + ");
            if (found >= 0) app(step, sizeof step, BTNW[found][0]);
            else if (!_stricmp(nm, "ST") || !_stricmp(nm, "START")) app(step, sizeof step, "START");
            else { char up[12]; int q; for (q = 0; nm[q]; q++) up[q] = (char)toupper((unsigned char)nm[q]); up[q] = 0; app(step, sizeof step, up); }
            p = e;
        }
        app(out, cap, step);
    }
}

static void lower_norm(const char *s, char *o, int cap) {   /* lower case, blanks and underscores become "-" */
    int n = 0;
    while (*s && n < cap - 1) { char ch = *s++; o[n++] = (ch == ' ' || ch == '_') ? '-' : (char)tolower((unsigned char)ch); }
    o[n] = 0;
}

static char *trim_dash(char *s) {
    char *e;
    while (*s == '-' || *s == ' ') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == '-' || e[-1] == ' ')) *--e = 0;
    return s;
}

/* one part of a step in words -> numpad notation; returns 0 when the words are not understood */
static int part_to_notation(char *part, int left, char *dig, char *btn, char *ms) {
    int k;
    char *at;
    part = trim_dash(part);
    if (!*part) return 1;
    if (!strcmp(part, "pause") || !strcmp(part, "~")) { strcat(dig, "~"); return 1; }
    if ((at = strstr(part, "@"))) { strncpy(ms, at + 1, 8); *at = 0; part = trim_dash(part); }
    if (!strncmp(part, "hold-", 5)) { part = trim_dash(part + 5); if (!*ms) strcpy(ms, "C"); }
    for (k = 0; k < (int)(sizeof MOTION / sizeof MOTION[0]); k++) {
        char m[60], ab[16];
        lower_norm(MOTION[k][1], m, sizeof m);
        lower_norm(MOTION[k][2], ab, sizeof ab);
        if (!strcmp(m, part) || !strcmp(ab, part)) { strcat(dig, MOTION[k][0]); return 1; }
    }
    if (!strcmp(part, "fc") || !strcmp(part, "full-circle")) { strcat(dig, "632147"); return 1; }   /* the older name of FCF */
    if (!strcmp(part, "fdp")) { strcat(dig, "623"); return 1; }   /* forward / back dragon punch: the same motions as DP / RDP */
    if (!strcmp(part, "bdp")) { strcat(dig, "421"); return 1; }
    for (k = 1; k <= 9; k++) {
        char d[20], a[8];
        lower_norm(DIRFULL[k], d, sizeof d);
        lower_norm(DIRW[k], a, sizeof a);
        if (!strcmp(d, part) || !strcmp(a, part)) { char c1[2] = {(char)('0' + (left ? mirror(k) : k)), 0}; strcat(dig, c1); return 1; }
    }
    {   /* facing-relative names */
        static const struct { const char *n, *a; char d; } REL[] = {{"forward", "f", '6'}, {"back", "b", '4'}, {"down-forward", "df", '3'}, {"down-back", "db", '1'}, {"up-forward", "uf", '9'}, {"up-back", "ub", '7'}};
        for (k = 0; k < 6; k++) if (!strcmp(REL[k].n, part) || !strcmp(REL[k].a, part)) { char c1[2] = {REL[k].d, 0}; strcat(dig, c1); return 1; }
    }
    if (part[0] == 'b' && part[1] == 'n' && part[2] >= '1' && part[2] <= '6' && !part[3]) {   /* BN1..BN6: the game buttons of the Controls tab */
        char t[4] = {'B', 'N', part[2], 0};
        if (btn[0]) strcat(btn, "+");
        strcat(btn, t);
        return 1;
    }
    if (!strcmp(part, "st") || !strcmp(part, "start")) { if (btn[0]) strcat(btn, "+"); strcat(btn, "ST"); return 1; }
    for (k = 0; k < 6; k++) {
        char b[20];
        lower_norm(BTNW[k][1], b, sizeof b);
        if (!strcmp(b, part) || !_stricmp(BTNW[k][0], part)) { if (btn[0]) strcat(btn, "+"); strcat(btn, BTNW[k][0]); return 1; }
    }
    return 0;
}

/* words -> numpad notation. Returns 0 when the text is not in words (it is then the short notation, or not understood) */
static int words_to_seq(const char *text, int left, char *out, int cap) {
    char t[600], flat[640], *step, *ctx1 = 0;
    int n = 0, depth = 0, i;
    static const char *KEY[] = {"down", "up", "left", "right", "neutral", "punch", "kick", "hold", "pause", "circle", "dragon", "forward", "back"};
    char low[600];
    int human = 0;
    for (i = 0; text[i] && i < 590; i++) low[i] = (char)tolower((unsigned char)text[i]);
    low[i] = 0;
    for (i = 0; i < (int)(sizeof KEY / sizeof KEY[0]); i++) if (strstr(low, KEY[i])) human = 1;
    {   /* the short direction names (d, dr, r, ...) on their own */
        static const char *ABBR[] = {"d", "dr", "dl", "r", "l", "u", "ur", "ul", "n", "f", "b", "df", "db", "uf", "ub", "qcf", "qcb", "dp", "rdp", "fdp", "bdp", "hcf", "hcb", "fc", "fcf", "fcb", "dfc", "dqcf", "dqcb"};
        char tk[600], *t1, *c1 = 0;
        strncpy(tk, low, sizeof tk - 1); tk[sizeof tk - 1] = 0;
        for (t1 = strtok_s(tk, ", +\t", &c1); t1; t1 = strtok_s(NULL, ", +\t", &c1)) {
            int a;
            for (a = 0; a < (int)(sizeof ABBR / sizeof ABBR[0]); a++) if (!strcmp(t1, ABBR[a])) human = 1;
        }
    }
    if (!human) return 0;
    /* parentheses are explanations, except "(2000 ms)" which becomes "@2000" */
    for (i = 0; low[i] && n < 590; i++) {
        if (low[i] == '(') {
            int v = atoi(low + i + 1);
            const char *close = strchr(low + i, ')');
            if (v > 0 && close && strstr(low + i, "ms") && strstr(low + i, "ms") < close) n += snprintf(flat + n, sizeof flat - (size_t)n, "@%d", v);
            depth = 1;
            continue;
        }
        if (low[i] == ')') { depth = 0; continue; }
        if (!depth) flat[n++] = low[i];
    }
    flat[n] = 0;
    { char *th; while ((th = strstr(flat, " then "))) memcpy(th, ",     ", 6); }
    out[0] = 0;
    strncpy(t, flat, sizeof t - 1); t[sizeof t - 1] = 0;
    for (step = strtok_s(t, ",", &ctx1); step; step = strtok_s(NULL, ",", &ctx1)) {
        char dig[700] = "", btn[700] = "", ms[10] = "", tmp[300], *part, *ctx2 = 0, sn[1500];
        strncpy(tmp, step, sizeof tmp - 1); tmp[sizeof tmp - 1] = 0;
        { char norm[300]; lower_norm(tmp, norm, sizeof norm); strcpy(tmp, norm); }
        for (part = strtok_s(tmp, "+", &ctx2); part; part = strtok_s(NULL, "+", &ctx2))
            if (!part_to_notation(part, left, dig, btn, ms) || strlen(dig) > 600 || strlen(btn) > 600) return 0;   /* (a part adds only a few characters: the arrays cannot overflow) */
        sn[0] = 0;
        if (!strcmp(dig, "~")) strcpy(sn, "~");
        else {
            strcat(sn, dig);
            if (ms[0] && dig[0]) { strcat(sn, "*"); strcat(sn, ms); }
            if (btn[0]) { if (dig[0]) strcat(sn, "+"); strcat(sn, btn); }
        }
        if (!sn[0]) continue;
        /* directions on their own, several in a row: one step each */
        if (!btn[0] && !ms[0] && strlen(dig) > 1 && strcmp(dig, "~")) { char sp[1500] = ""; for (i = 0; dig[i]; i++) { char c1[3] = {dig[i], ' ', 0}; strcat(sp, c1); } strcpy(sn, sp); }
        if (out[0]) app(out, cap, " ");
        app(out, cap, sn);
    }
    { size_t l = strlen(out); while (l && out[l - 1] == ' ') out[--l] = 0; }
    return 1;
}

static void txt8(const char *s, wchar_t *out, int cap) {
    wchar_t *w = u8_to_w(s, -1);
    wcsncpy(out, w, cap - 1);
    out[cap - 1] = 0;
    free(w);
}
static void dash8(const char *s, wchar_t *out, int cap) { if (!s[0]) wcscpy(out, L"-"); else txt8(s, out, cap); }


/* ---------------------------------------------------------------- the move lists */
static const ComboMove genericMoves[] = {
    {"Throw", "Throw (forward + MP)", "6+MP", "When close, f + MP"},
    {"Throw", "Throw (forward + MK)", "6+MK", "When close, f + MK"},
    {"Guard Break", "Guard Break (HP + HK)", "HP+HK", "same strength P + K (costs 1 level)"},
    {"Excel", "Excel activation (LP + MK)", "LP+MK", "different strength P + K (costs 1 level)"},
    {"Special", "Fireball (QCF + HP)", "236+HP", "qcf + P"},
    {"Special", "Dragon punch (DP + HP)", "623+HP", "f,d,df + P"},
    {"Special", "Hurricane kick (QCB + HK)", "214+HK", "qcb + K"},
    {"Super", "Super fireball (QCF x2 + HP)", "236236+HP", "qcf,qcf + P"},
    {"Super", "Super dragon punch (QCF, DP + HP)", "23623+HP", "qcf,d,df + P"},
    {"Super", "Super hurricane kick (QCB x2 + HK)", "214214+HK", "qcb,qcb + K"},
    {"Super", "Fireball > super cancel", "236+MP ~ ~ 236236+HP", "qcf + MP, then qcf,qcf + HP"},
    {"Meteor", "Meteor combo (QCF x2 + PPP)", "236236+LP+MP+HP", "qcf,qcf + PPP"},
    {"Meteor", "Meteor combo (QCB x2 + PPP)", "214214+LP+MP+HP", "qcb,qcb + PPP"},
    {"Meteor", "Meteor combo (QCF x2 + KKK)", "236236+LK+MK+HK", "qcf,qcf + KKK"},
    {"Meteor", "Meteor combo (QCB x2 + KKK)", "214214+LK+MK+HK", "qcb,qcb + KKK"},
    {"Meteor", "Meteor combo (720 + PPP)", "632147632147+LP+MP+HP", "rotate 720 + PPP"},
};

static int combo_group_count(void) { return g_nComboChars + 1; }     /* group 0 is "Generic Moves" */
static const char *combo_group(int i, const ComboMove **moves, int *n) {
    if (i == 0) { *moves = genericMoves; *n = (int)(sizeof genericMoves / sizeof genericMoves[0]); return "Generic Moves"; }
    *moves = g_comboChars[i - 1].moves;
    *n = g_comboChars[i - 1].nmoves;
    return g_comboChars[i - 1].name;
}

static int has_motion(const char *s) {   /* three or more digits in a row */
    int run = 0;
    for (; *s; s++) { if (isdigit((unsigned char)*s)) { if (++run >= 3) return 1; } else run = 0; }
    return 0;
}

/* every move of a character, most important first: Meteor Combos, Super Combos, motion specials, other special / command attacks, Guard Break, throws */
static void combos_apply_loadout(const char *charName) {
    const ComboMove *tier[6][40];
    int nt[6] = {0}, c, i, t, k = 0;
    const ComboMove *ordered[NUM_COMBOS * 4];
    int no = 0;
    for (c = 0; c < g_nComboChars; c++) {
        if (strcmp(g_comboChars[c].name, charName)) continue;
        for (i = 0; i < g_comboChars[c].nmoves; i++) {
            const ComboMove *m = &g_comboChars[c].moves[i];
            int ti;
            if (!strcmp(m->kind, "Meteor")) ti = 0;
            else if (!strcmp(m->kind, "Super")) ti = 1;
            else if (!strcmp(m->kind, "Special")) ti = (strstr(m->seq, "*C") || has_motion(m->seq)) ? 2 : 3;
            else if (!strcmp(m->kind, "Guard Break")) ti = 4;
            else ti = 5;
            if (nt[ti] < 40) tier[ti][nt[ti]++] = m;
        }
    }
    for (t = 0; t < 6; t++) for (i = 0; i < nt[t]; i++) if (no < NUM_COMBOS * 4) ordered[no++] = tier[t][i];
    for (k = 0; k < NUM_COMBOS; k++) {
        ComboSlot *s = &g_in.slots[k];
        s->name[0] = s->kind[0] = s->seq[0] = 0;
        if (k < no) {
            _snprintf(s->name, sizeof s->name, "%s: %s", charName, ordered[k]->name);
            s->name[sizeof s->name - 1] = 0;
            strncpy(s->kind, ordered[k]->kind, sizeof s->kind - 1);
            strncpy(s->seq, ordered[k]->seq, sizeof s->seq - 1);
        }
    }
}

/* the tab as it starts: default step and charge times, no triggers, the Ryu move list */
static void combos_defaults(void) {
    int i;
    g_in.stepMs = 30;
    g_in.chargeMs = 600;
    for (i = 0; i < NUM_COMBOS; i++) { ComboSlot *s = &g_in.slots[i]; memset(s, 0, sizeof *s); s->player = 1; if (i < 9) { s->key[0] = (char)('1' + i); s->key[1] = 0; } }   /* the keys 1 ... 9 */
    combos_apply_loadout("Ryu");
}

/* ---------------------------------------------------------------- mame-ex-combos.cfg */
static void cfg_path(wchar_t *out, size_t cap) { path_join(out, cap, g_root, L"mame-ex-combos.cfg"); }

static int g_cfgLoaded;

static void cmb_show(void) {   /* the values of g_in into the controls */
    if (g_cmb.lv) {
        CmbUI *u = &g_cmb;
        edit_set_int(u->eChg, g_in.chargeMs);
        edit_set_int(u->eStep, g_in.stepMs);
        cmb_refresh();
        fill_form();
    }
}

void cmb_load(void) {   /* the file into g_in (no file: the defaults), then into the controls */
    wchar_t p[560];
    char *txt, *line, *ctx = NULL;
    g_cfgLoaded = 1;
    combos_defaults();
    cfg_path(p, 560);
    txt = read_file(p, NULL);
    if (txt) {
        for (line = strtok_s(txt, "\r\n", &ctx); line; line = strtok_s(NULL, "\r\n", &ctx)) {
            char *eq = strchr(line, '='), *v;
            int n;
            if (!eq) continue;
            *eq = 0; v = eq + 1;
            if (!strcmp(line, "step_ms")) { n = atoi(v); if (n >= 16 && n <= 500) g_in.stepMs = n; }
            else if (!strcmp(line, "charge_ms")) { n = atoi(v); if (n >= 100 && n <= 3000) g_in.chargeMs = n; }
            else if (!strncmp(line, "combo", 5) && line[5] >= '0' && line[5] <= '9') {
                const char *rest = line + 5;
                ComboSlot *s;
                n = atoi(rest);
                while (*rest >= '0' && *rest <= '9') rest++;
                if (n < 1 || n > NUM_COMBOS || *rest != '_') continue;
                s = &g_in.slots[n - 1];
                rest++;
                if (!strcmp(rest, "name")) strncpy(s->name, v, sizeof s->name - 1);
                else if (!strcmp(rest, "kind")) strncpy(s->kind, v, sizeof s->kind - 1);
                else if (!strcmp(rest, "seq")) strncpy(s->seq, v, sizeof s->seq - 1);
                else if (!strcmp(rest, "player")) s->player = atoi(v) == 2 ? 2 : 1;
                else if (!strcmp(rest, "side")) s->faceLeft = v[0] == 'R' || v[0] == 'r';   /* the side of the screen: on the right the character looks left */
                else if (!strcmp(rest, "key")) strncpy(s->key, v, sizeof s->key - 1);
                else if (!strcmp(rest, "pad")) strncpy(s->pad, v, sizeof s->pad - 1);
            }
        }
        free(txt);
    }
    {   /* a file from before the default keys has no trigger at all: the keys 1 ... 9 */
        int i, any = 0;
        for (i = 0; i < NUM_COMBOS; i++) if (g_in.slots[i].key[0] || g_in.slots[i].pad[0]) any = 1;
        if (!any) for (i = 0; i < 9; i++) { g_in.slots[i].key[0] = (char)('1' + i); g_in.slots[i].key[1] = 0; }
    }
    cmb_show();
}

void cmb_save(void) {
    wchar_t p[560];
    Buf b = {0};
    int i;
    cmb_collect();
    buf_fmt(&b, "step_ms=%d\r\ncharge_ms=%d\r\n", g_in.stepMs, g_in.chargeMs);
    for (i = 0; i < NUM_COMBOS; i++) {
        const ComboSlot *s = &g_in.slots[i];
        buf_fmt(&b, "combo%d_name=%s\r\ncombo%d_kind=%s\r\ncombo%d_seq=%s\r\ncombo%d_player=%d\r\ncombo%d_side=%s\r\ncombo%d_key=%s\r\ncombo%d_pad=%s\r\n",
                i + 1, s->name, i + 1, s->kind, i + 1, s->seq, i + 1, s->player, i + 1, s->faceLeft ? "R" : "L", i + 1, s->key, i + 1, s->pad);
    }
    cfg_path(p, 560);
    write_file(p, b.s, b.n);
    buf_free(&b);
}

static void row_texts(int i) {
    ComboSlot *s = &g_in.slots[i];
    wchar_t t[180];
    swprintf(t, 180, L"%d", i + 1); ListView_SetItemText(g_cmb.lv, i, 0, t);
    txt8(s->name, t, 180); ListView_SetItemText(g_cmb.lv, i, 1, t);
    txt8(s->kind, t, 180); ListView_SetItemText(g_cmb.lv, i, 2, t);
    { char w[400]; wchar_t tw[400]; seq_to_words(s->seq, s->faceLeft, w, sizeof w); txt8(w, tw, 400); ListView_SetItemText(g_cmb.lv, i, 3, tw); }
    swprintf(t, 180, L"Player %d", s->player); ListView_SetItemText(g_cmb.lv, i, 4, t);
    ListView_SetItemText(g_cmb.lv, i, 5, (LPWSTR)(s->faceLeft ? L"Right" : L"Left"));   /* the side of the screen */
    key_display(s->key, t, 180); ListView_SetItemText(g_cmb.lv, i, 6, t);
    pad_display(s->pad, t, 180); ListView_SetItemText(g_cmb.lv, i, 7, t);
}

static int cur_row(void) { return ListView_GetNextItem(g_cmb.lv, -1, LVNI_SELECTED); }
static ComboSlot *cur_slot(void) { int i = cur_row(); return (i >= 0 && i < g_rows) ? &g_in.slots[i] : NULL; }

static void fill_form(void);

void cmb_refresh(void) {
    CmbUI *u = &g_cmb;
    int n = NUM_COMBOS, i, cur = cur_row();
    if (n != g_rows) {
        SendMessageW(u->lv, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(u->lv);
        for (i = 0; i < n; i++) { LVITEMW it; memset(&it, 0, sizeof it); it.mask = LVIF_TEXT; it.iItem = i; it.pszText = (LPWSTR)L""; ListView_InsertItem(u->lv, &it); }
        g_rows = n;
        if (cur >= 0 && cur < n) ListView_SetItemState(u->lv, cur, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        SendMessageW(u->lv, WM_SETREDRAW, TRUE, 0);
    }
    for (i = 0; i < g_rows; i++) row_texts(i);
    InvalidateRect(u->lv, NULL, TRUE);
}

/* ---------------------------------------------------------------- pickers */
static void set_seq_edit(const ComboSlot *s) {
    char w[400];
    seq_to_words(s->seq, s->faceLeft, w, sizeof w);
    edit_set_text_utf8(g_cmb.eSeq, w);
}

static void set_note(const char *cmd) {
    wchar_t t[300];
    txt8(cmd ? cmd : "", t, 300);
    CharUpperW(t);   /* QCF,QCF + KKK */
    SetWindowTextW(g_cmb.note, t);
}

static void show_char(int ci) {
    CmbUI *u = &g_cmb;
    const ComboMove *mv;
    int n, i;
    if (ci < 0 || ci >= combo_group_count()) return;
    combo_group(ci, &mv, &n);
    SendMessageW(u->cbMove, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++) {
        char a[500];
        wchar_t *w;
        char ws[300];
        ComboSlot *cs = cur_slot();
        seq_to_words(mv[i].seq, cs ? cs->faceLeft : 0, ws, sizeof ws);
        _snprintf(a, sizeof a, "[%s] %s   (%s)", mv[i].kind, mv[i].name, ws);
        a[sizeof a - 1] = 0;
        w = u8_to_w(a, -1);
        SendMessageW(u->cbMove, CB_ADDSTRING, 0, (LPARAM)w);
        free(w);
    }
    SendMessageW(u->cbMove, CB_SETCURSEL, (WPARAM)-1, 0);
}

static void fill_form(void) {
    CmbUI *u = &g_cmb;
    ComboSlot *s = cur_slot();
    int ci, mi = -1, k;
    char *colon;
    u->syncing = 1;
    if (!s) { SetWindowTextW(u->eSeq, L""); set_note(NULL); u->syncing = 0; return; }
    set_seq_edit(s);
    SendMessageW(u->cbPlay, CB_SETCURSEL, s->player - 1, 0);
    SendMessageW(u->cbFace, CB_SETCURSEL, s->faceLeft ? 1 : 0, 0);
    ci = (int)SendMessageW(u->cbChar, CB_GETCURSEL, 0, 0);
    /* "Character: Move" names select the matching entries in the two pickers */
    colon = strstr(s->name, ": ");
    if (colon && colon > s->name) {
        char grp[160];
        int g;
        size_t gl = colon - s->name;
        memcpy(grp, s->name, gl);
        grp[gl] = 0;
        for (g = 0; g < combo_group_count(); g++) {
            const ComboMove *mv; int n;
            const char *gn = combo_group(g, &mv, &n);
            if (_stricmp(gn, grp)) continue;
            ci = g;
            show_char(ci);
            for (k = 0; k < n; k++) if (!strcmp(mv[k].name, colon + 2)) mi = k;
        }
    }
    if (ci < 0) ci = 0;
    SendMessageW(u->cbChar, CB_SETCURSEL, ci, 0);
    SendMessageW(u->cbMove, CB_SETCURSEL, mi, 0);
    {
        const ComboMove *mv; int n;
        if (mi >= 0 && ci >= 0 && ci < combo_group_count()) { combo_group(ci, &mv, &n); set_note(mi < n ? mv[mi].cmd : NULL); } else set_note(NULL);
    }
    u->syncing = 0;
}

void cmb_collect(void) {
    int v = edit_int(g_cmb.eStep, 0, 500);
    if (v >= 16) g_in.stepMs = v;
    v = edit_int(g_cmb.eChg, 0, 3000);
    if (v >= 100) g_in.chargeMs = v;
}

/* a click on a trigger cell */
void cmb_cell(int row, int col) {
    ComboSlot *s;
    wchar_t label[60];
    if (row < 0 || row >= g_rows || col < 6 || col > 7) return;
    s = &g_in.slots[row];
    swprintf(label, 60, L"Combo %d Trigger", row + 1);
    if (col == 6) { int cl = 0; capture_key_token(ui_owner(g_main), label, s->key, &cl); }
    else capture_pad_token(ui_owner(g_main), label, 0, s->pad);
    cmb_refresh();
}

static void load_character(void) {
    CmbUI *u = &g_cmb;
    int i = (int)SendMessageW(u->cbLoad, CB_GETCURSEL, 0, 0);
    if (u->loading || i < 0 || i >= g_nComboChars) return;
    u->loading = 1;
    combos_apply_loadout(g_comboChars[i].name);
    cmb_refresh();
    ListView_SetItemState(u->lv, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    fill_form();
    SendMessageW(u->cbLoad, CB_SETCURSEL, (WPARAM)-1, 0);   /* cleared, so the same character can be picked again */
    u->loading = 0;
}

/* a button under the Sequence box: its text goes to the end of the Sequence, with the separator that fits */
static int is_button_word(const wchar_t *w) {
    static const wchar_t *B[] = {L"LP", L"MP", L"HP", L"LK", L"MK", L"HK", L"START", L"ST", NULL};
    int k;
    for (k = 0; B[k]; k++) if (!_wcsicmp(B[k], w)) return 1;
    return (w[0] == L'B' || w[0] == L'b') && (w[1] == L'N' || w[1] == L'n') && w[2] >= L'1' && w[2] <= L'6' && !w[3];
}

static void wtrim_end(wchar_t *t, size_t *n) { while (*n && t[*n - 1] == L' ') t[--*n] = 0; }

/* a button under the Sequence box: its text goes into the Sequence at the caret (or over the selection), with the separator that fits */
static void token_click(int i) {
    CmbUI *u = &g_cmb;
    wchar_t t[400], before[400], after[400], tok[16], res[800];
    DWORD s0 = 0, s1 = 0;
    size_t n, nb, caret;
    const wchar_t *a;
    if (!cur_slot()) {   /* no row chosen: the first row without a sequence (the first row when all are used) */
        int r, pick = 0;
        for (r = 0; r < g_rows; r++) if (!g_in.slots[r].seq[0]) { pick = r; break; }
        ListView_SetItemState(u->lv, pick, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(u->lv, pick, FALSE);
        if (!cur_slot()) return;
    }
    GetWindowTextW(u->eSeq, t, 400);
    n = wcslen(t);
    SendMessageW(u->eSeq, EM_GETSEL, (WPARAM)&s0, (LPARAM)&s1);
    if (s0 > n) s0 = (DWORD)n;
    if (s1 > n || s1 < s0) s1 = (DWORD)n;
    wcsncpy(before, t, s0); before[s0] = 0;
    wcscpy(after, t + s1);
    nb = wcslen(before);
    wtrim_end(before, &nb);
    for (a = after; *a == L' '; a++) ;
    memmove(after, a, (wcslen(a) + 1) * sizeof(wchar_t));
    txt8(TOKS[i], tok, 16);
    if (!strcmp(TOKS[i], "Del")) {   /* the token before the caret, a motion's (...) with it */
        if (nb && (before[nb - 1] == L',' || before[nb - 1] == L'+')) before[--nb] = 0;
        wtrim_end(before, &nb);
        if (nb && before[nb - 1] == L')') { while (nb && before[nb - 1] != L'(') nb--; if (nb) nb--; while (nb && before[nb - 1] == L' ') nb--; }
        while (nb && before[nb - 1] != L',' && before[nb - 1] != L'+' && before[nb - 1] != L' ' && before[nb - 1] != L'(') nb--;
        while (nb && (before[nb - 1] == L' ' || before[nb - 1] == L',' || before[nb - 1] == L'+')) nb--;
        before[nb] = 0;
        caret = nb;
        if (!after[0]) _snwprintf(res, 800, L"%ls", before);
        else if (!nb) { const wchar_t *r = after; while (*r == L',' || *r == L'+' || *r == L' ') r++; _snwprintf(res, 800, L"%ls", r); }
        else _snwprintf(res, 800, L"%ls%ls%ls", before, after[0] == L',' ? L"" : after[0] == L'+' ? L" " : L", ", after);
    } else {
        wchar_t sepB[8] = L"", sepA[8] = L"";
        int endsSep = !nb || before[nb - 1] == L',' || before[nb - 1] == L'+';
        if (!wcscmp(tok, L"+")) {
            if (endsSep) tok[0] = 0;   /* nothing to join yet, or already joined */
            else { wcscpy(sepB, L" "); if (after[0] && after[0] != L'+') wcscpy(sepA, L" "); }
        } else if (!wcscmp(tok, L",")) {
            if (endsSep) tok[0] = 0;
            else if (after[0] && after[0] != L',') wcscpy(sepA, L" ");
        } else {
            if (endsSep) { if (nb) wcscpy(sepB, L" "); }
            else {   /* a button after a direction or a motion is pressed with it, anything else is the next step */
                size_t e = nb, b0;
                wchar_t last[24];
                while (e && before[e - 1] != L',' && before[e - 1] != L'+' && before[e - 1] != L' ' && before[e - 1] != L')') e--;
                b0 = e;
                last[0] = 0;
                if (before[nb - 1] != L')') { size_t l = nb - b0; if (l > 0 && l < 24) { wmemcpy(last, before + b0, l); last[l] = 0; } }
                if (!_wcsicmp(last, L"Hold")) wcscpy(sepB, L" ");   /* Hold, then the direction that is held */
                else wcscpy(sepB, (is_button_word(tok) && (before[nb - 1] == L')' || (last[0] && !is_button_word(last) && wcscmp(last, L"~")))) ? L" + " : L", ");
            }
            if (after[0]) wcscpy(sepA, !_wcsicmp(tok, L"Hold") ? L" " : after[0] == L',' ? L"" : after[0] == L'+' ? L" " : L", ");
        }
        caret = nb + wcslen(sepB) + wcslen(tok);
        _snwprintf(res, 800, L"%ls%ls%ls%ls%ls", before, sepB, tok, sepA, after);
    }
    res[399] = 0;
    SetWindowTextW(u->eSeq, res);   /* EN_CHANGE reads it */
    SetFocus(u->eSeq);
    SendMessageW(u->eSeq, EM_SETSEL, (WPARAM)caret, (LPARAM)caret);
}

int cmb_command(int id, int code) {
    CmbUI *u = &g_cmb;
    ComboSlot *s;
    if (id == ID_K_LEGEND) { if (code == BN_CLICKED) dlg_legend(); return 1; }
    if (id == ID_K_HOW) { if (code == BN_CLICKED) msg_box(g_main, L"How Combos Work", INFO, MB_ICONINFORMATION, NULL); return 1; }
    if (id >= ID_K_TOK && id < ID_K_TOK + CMB_NTOK) { if (code == BN_CLICKED) token_click(id - ID_K_TOK); return 1; }
    if (id == ID_K_STEP || id == ID_K_CHG) { if (code == EN_CHANGE) cmb_collect(); return 1; }
    if (id == ID_K_LOADCHAR && code == CBN_SELCHANGE) { load_character(); return 1; }
    if (id == ID_K_CHAR && code == CBN_SELCHANGE) {
        if (!u->syncing && SendMessageW(u->cbChar, CB_GETCURSEL, 0, 0) >= 0) show_char((int)SendMessageW(u->cbChar, CB_GETCURSEL, 0, 0));
        return 1;
    }
    if (id == ID_K_MOVE && code == CBN_SELCHANGE) {
        int ci = (int)SendMessageW(u->cbChar, CB_GETCURSEL, 0, 0), mi = (int)SendMessageW(u->cbMove, CB_GETCURSEL, 0, 0);
        const ComboMove *mv; int n;
        const char *gn;
        if (u->syncing || ci < 0 || mi < 0 || ci >= combo_group_count()) return 1;
        gn = combo_group(ci, &mv, &n);
        if (mi >= n || !(s = cur_slot())) return 1;
        _snprintf(s->name, sizeof s->name, "%s: %s", gn, mv[mi].name);
        s->name[sizeof s->name - 1] = 0;
        strncpy(s->kind, mv[mi].kind, sizeof s->kind - 1);
        strncpy(s->seq, mv[mi].seq, sizeof s->seq - 1);
        u->syncing = 1;
        set_seq_edit(s);
        set_note(mv[mi].cmd);
        u->syncing = 0;
        cmb_refresh();
        return 1;
    }
    if (id == ID_K_SEQ && code == EN_CHANGE) {
        if (u->syncing || !(s = cur_slot())) return 1;
        {   /* words become the short notation the plugin reads; anything else is kept as typed */
            char raw[400], seq[400];
            edit_get_text_utf8(u->eSeq, raw, sizeof raw);
            strncpy(s->seq, words_to_seq(raw, s->faceLeft, seq, sizeof seq) ? seq : raw, sizeof s->seq - 1);
            s->seq[sizeof s->seq - 1] = 0;
        }
        set_note(NULL);
        strcpy(s->name, "Custom");
        strcpy(s->kind, "Custom");
        u->syncing = 1;
        SendMessageW(u->cbMove, CB_SETCURSEL, (WPARAM)-1, 0);
        u->syncing = 0;
        cmb_refresh();
        return 1;
    }
    if (id == ID_K_PLAY && code == CBN_SELCHANGE) {
        int i = (int)SendMessageW(u->cbPlay, CB_GETCURSEL, 0, 0);
        if ((s = cur_slot()) && !u->syncing && i >= 0) { s->player = i + 1; cmb_refresh(); }
        return 1;
    }
    if (id == ID_K_FACE && code == CBN_SELCHANGE) {
        int i = (int)SendMessageW(u->cbFace, CB_GETCURSEL, 0, 0);
        if ((s = cur_slot()) && !u->syncing && i >= 0) {
            int ci = (int)SendMessageW(u->cbChar, CB_GETCURSEL, 0, 0), mi = (int)SendMessageW(u->cbMove, CB_GETCURSEL, 0, 0);
            s->faceLeft = i == 1;
            u->syncing = 1;   /* Left / Right in the words swap with the player side */
            set_seq_edit(s);
            if (ci >= 0) { show_char(ci); SendMessageW(u->cbMove, CB_SETCURSEL, mi, 0); }
            u->syncing = 0;
            cmb_refresh();
        }
        return 1;
    }
    if (id == ID_K_DEF && code == BN_CLICKED) {
        int r = 0;
        msg_box(ui_owner(g_main), L"Combo Defaults", L"Restore the combos to their defaults?\n\nAll rows, triggers and the step / charge times of the Combos tab go back to the start values (the keys 1 ... 9 as triggers). Other tabs are not touched.", MB_YESNO | MB_ICONQUESTION, &r);
        if (r == IDYES) { combos_defaults(); cmb_show(); ListView_SetItemState(u->lv, -1, 0, LVIS_SELECTED | LVIS_FOCUSED); fill_form(); }   /* no row chosen: the sequence buttons then use the first empty row */
        return 1;
    }
    if (id == ID_K_CLEAR && code == BN_CLICKED) {
        if ((s = cur_slot())) { memset(s, 0, sizeof *s); s->player = 1; cmb_refresh(); fill_form(); }
        return 1;
    }
    return 0;
}

int cmb_notify(NMHDR *nh) {
    if (nh->idFrom != ID_K_LV) return 0;
    if (nh->code == NM_CLICK) {
        int row, col;
        if (lv_hit_cell(g_cmb.lv, (LPARAM)nh, &row, &col)) {
            if (row < g_rows) ListView_SetItemState(g_cmb.lv, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            if (col == 6 || col == 7) PostMessageW(g_main, WM_APP_CELL, 1, MAKELPARAM(row, col));
        }
        return 1;
    }
    if (nh->code == NM_DBLCLK) {   /* a double click on the name renames the combo */
        int row, col;
        if (lv_hit_cell(g_cmb.lv, (LPARAM)nh, &row, &col) && row < g_rows && col == 1) {
            ComboSlot *s = &g_in.slots[row];
            wchar_t t[160];
            ListView_SetItemState(g_cmb.lv, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            txt8(s->name, t, 160);
            if (dlg_text(ui_owner(g_main), L"Rename Combo", L"Name of this combo:", t, 100)) {
                char *u = w_to_u8(t);
                strncpy(s->name, u ? u : "", sizeof s->name - 1);
                s->name[sizeof s->name - 1] = 0;
                free(u);
                cmb_refresh();
            }
        }
        return 1;
    }
    if (nh->code == NM_RCLICK) {   /* a right click on a trigger cell: menu with Set and Clear */
        int row, col;
        if (lv_hit_cell(g_cmb.lv, (LPARAM)nh, &row, &col) && row < g_rows && (col == 6 || col == 7)) {
            ComboSlot *s = &g_in.slots[row];
            int pick;
            ListView_SetItemState(g_cmb.lv, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            {
                HMENU m = CreatePopupMenu();
                POINT pt;
                GetCursorPos(&pt);
                AppendMenuW(m, MF_STRING, 1, L"Set…");
                AppendMenuW(m, MF_STRING, 2, L"Clear");
                pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, NULL);
                DestroyMenu(m);
            }
            if (pick == 2) { if (col == 6) s->key[0] = 0; else s->pad[0] = 0; cmb_refresh(); }
            else if (pick == 1) PostMessageW(g_main, WM_APP_CELL, 1, MAKELPARAM(row, col));
        }
        return 1;
    }
    if (nh->code == LVN_ITEMCHANGED) {
        NMLISTVIEW *lv = (NMLISTVIEW *)nh;
        if ((lv->uChanged & LVIF_STATE) && (lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED)) fill_form();
        return 1;
    }
    if (nh->code == NM_CUSTOMDRAW) return -1;
    return 0;
}
/* ---------------------------------------------------------------- creating the controls */
static void place(HWND h, int x, int y, int w, int hh) { SetWindowPos(h, NULL, x, y, w, hh, SWP_NOZORDER); }

void cmb_create(HWND page, HWND tip) {
    static const wchar_t *cols[] = {L"#", L"Name", L"Type", L"Sequence", L"Player", L"Side", L"Keyboard", L"Pad"};
    static const int widths[] = {28, 100, 0, 150, 0, 0, 66, 86};   /* Type, Player and Side are shown by the boxes under the list */
    static const wchar_t *plays[] = {L"Player 1", L"Player 2"}, *faces[] = {L"Left", L"Right"};   /* the side of the screen: on the left the character looks right */
    CmbUI *u = &g_cmb;
    int i;
    u->page = page;
#define TP(h, text) set_tip(tip, page, h, text)
    u->intro = mk(page, L"STATIC", L"Give a row a trigger (click its Keyboard or Pad cell): while a game runs, pressing it plays the move of that row for you. Double-click a name to rename it.", SS_LEFT, 0, 0);
    u->bHow = mk(page, L"BUTTON", L"How It Works", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_K_HOW);
    u->bLegend = mk(page, L"BUTTON", L"Legend", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_K_LEGEND);
    TP(u->bLegend, L"What the short names (QCF, HP, DR, BN1 ...) stand for.");
    TP(u->bHow, L"How the triggers, the sequences and the charge moves work.");
    u->lStep = mk(page, L"STATIC", L"Step (ms)", SS_LEFT, 0, 0);
    u->eStep = mk(page, L"EDIT", L"", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_K_STEP);
    TP(u->eStep, L"How long each step of a sequence is held (default 30 ms). The game must see a step for at least one frame: with a slow game raise it.");
    u->lChg = mk(page, L"STATIC", L"Charge (ms)", SS_LEFT, 0, 0);
    u->eChg = mk(page, L"EDIT", L"", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_K_CHG);
    TP(u->eChg, L"How long \"Hold\" (4*C) steps keep a direction pressed: charge moves (Guile, Blanka ...). Default 600 ms; raise it a little if a charge move does not come out.");
    u->lLoad = mk(page, L"STATIC", L"Load Character", SS_LEFT, 0, 0);
    u->cbLoad = mk(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_K_LOADCHAR);
    TP(u->cbLoad, L"Puts every move of a character (Street Fighter EX2 Plus move lists) into the rows, most important first. The triggers stay.");
    for (i = 0; i < g_nComboChars; i++) { wchar_t t[80]; txt8(g_comboChars[i].name, t, 80); SendMessageW(u->cbLoad, CB_ADDSTRING, 0, (LPARAM)t); }
    SendMessageW(u->cbLoad, CB_SETCURSEL, (WPARAM)-1, 0);
    u->lv = lv_create(page, ID_K_LV, cols, widths, 8);
    u->lChar = mk(page, L"STATIC", L"Character", SS_LEFT, 0, 0);
    u->cbChar = mk(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_K_CHAR);
    for (i = 0; i < combo_group_count(); i++) {
        const ComboMove *mv; int n; wchar_t t[80];
        txt8(combo_group(i, &mv, &n), t, 80);
        SendMessageW(u->cbChar, CB_ADDSTRING, 0, (LPARAM)t);
    }
    SendMessageW(u->cbChar, CB_SETCURSEL, 0, 0);
    TP(u->cbChar, L"The character whose move list the Move box shows (Generic Moves: moves that most games share).");
    u->lMove = mk(page, L"STATIC", L"Move", SS_LEFT, 0, 0);
    u->cbMove = mk(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_K_MOVE);
    TP(u->cbMove, L"Choose a move: it goes into the chosen row.");
    u->lSeq = mk(page, L"STATIC", L"Sequence", SS_LEFT, 0, 0);
    u->eSeq = mk(page, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_K_SEQ);
    TP(u->eSeq, L"The moves in words, e.g. Down, Down-Right, Right + Heavy Punch (or QCF + HP, or 236+HP). Steps are separated by commas, + presses together.");
    for (i = 0; i < CMB_NTOK; i++) {
        wchar_t t[12];
        if (!TOKS[i]) { u->tok[i] = NULL; continue; }
        HICON ico = token_icon(TOKS[i]);
        txt8(TOKS[i], t, 12);
        u->tok[i] = mk(page, L"BUTTON", ico ? L"" : t, BS_PUSHBUTTON | WS_TABSTOP | (ico ? BS_ICON : 0), 0, ID_K_TOK + i);
        if (ico) SendMessageW(u->tok[i], BM_SETIMAGE, IMAGE_ICON, (LPARAM)ico);
    }
    for (i = 0; i < CMB_NTOK; i++) {   /* what each button stands for */
        static const struct { const char *k, *t; } TT[] = {
            {"U", "Up"}, {"D", "Down"}, {"L", "Left"}, {"R", "Right"}, {"UL", "Up-Left"}, {"UR", "Up-Right"}, {"DL", "Down-Left"}, {"DR", "Down-Right"},
            {"LP", "Light Punch (Button 1)"}, {"MP", "Medium Punch (Button 2)"}, {"HP", "Heavy Punch (Button 3)"},
            {"LK", "Light Kick (Button 4)"}, {"MK", "Medium Kick (Button 5)"}, {"HK", "Heavy Kick (Button 6)"}, {"START", "Start button"},
            {"+", "Pressed together: written between buttons / directions of one step (QCF + HP)"}, {",", "Starts the next step"}, {"~", "A short wait between two steps: one neutral step as long as Step (ms)"},
            {"Hold", "Hold the next direction for the Charge time (charge moves): Hold, then a direction, e.g. Hold DL, R + HP"},
            {"QCF", "Quarter-circle Forward (D, DR, R)"}, {"QCB", "Quarter-circle Back (D, DL, L)"},
            {"RDP", "Reverse Dragon-punch Motion (L, D, DL)"}, {"HCF", "Half-circle Forward"}, {"HCB", "Half-circle Back"},
            {"FCF", "Full Circle Forward (R, DR, D, DL, L, UL)"}, {"FCB", "Full Circle Back (L, DL, D, DR, R, UR)"},
            {"Del", "Removes the last step, button or direction of the Sequence"}};
        int k;
        wchar_t tx[120];
        for (k = 0; TOKS[i] && k < (int)(sizeof TT / sizeof TT[0]); k++) if (!strcmp(TT[k].k, TOKS[i])) { txt8(TT[k].t, tx, 120); set_tip(tip, page, u->tok[i], tx); }
    }
    u->lPlay = mk(page, L"STATIC", L"Plays For", SS_LEFT, 0, 0);
    u->cbPlay = mk(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, ID_K_PLAY);
    combo_fill(u->cbPlay, plays, 2);
    SendMessageW(u->cbPlay, CB_SETCURSEL, 0, 0);
    TP(u->cbPlay, L"The player whose buttons the combo presses.");
    u->lFace = mk(page, L"STATIC", L"Player Side", SS_LEFT, 0, 0);
    u->cbFace = mk(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 0, ID_K_FACE);
    combo_fill(u->cbFace, faces, 2);
    SendMessageW(u->cbFace, CB_SETCURSEL, 0, 0);
    TP(u->cbFace, L"Left (default): your character is on the left of the screen and looks right. Right: it is on the right and looks left; Left / Right then swap in the sequence.");
    u->bClear = mk(page, L"BUTTON", L"Clear Slot", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_K_CLEAR);
    u->bDef = mk(page, L"BUTTON", L"Defaults", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_K_DEF);
    TP(u->bDef, L"Gives the combos their start values: the Ryu move list, the keys 1 ... 9 as triggers.");
    u->note = mk(page, L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, 0, 0);   /* the move's note from the move list (how it is done in the game) */
#undef TP
    g_rows = 0;
    show_char(0);
    cmb_load();
}

void cmb_layout(int w, int h) {
    CmbUI *u = &g_cmb;
    int m = S(8), x = m, y = m, W = w - 2 * m, bh = S(26), rh = S(24), lw, form, fy, half, ih;
    if (!u->lv) return;
    ih = S(52);
    place(u->intro, x, y, W - S(100), ih);
    place(u->bHow, x + W - S(96), y, S(96), bh);
    place(u->bLegend, x + W - S(96), y + bh + S(4), S(96), bh);
    y += ih + S(6);
    lw = text_width(u->page, L"Step (ms)") + S(10);
    place(u->lStep, x, y + S(4), lw, S(18));
    place(u->eStep, x + lw, y, S(60), rh);
    place(u->lChg, x + lw + S(80), y + S(4), text_width(u->page, L"Charge (ms)") + S(10), S(18));
    place(u->eChg, x + lw + S(80) + text_width(u->page, L"Charge (ms)") + S(10), y, S(60), rh);
    y += S(30);
    lw = text_width(u->page, L"Load Character") + S(10);
    place(u->lLoad, x, y + S(4), lw, S(18));
    place(u->cbLoad, x + lw, y, S(260), S(260));
    y += S(32);
    form = 4 * S(28) + 3 * S(32) + S(30) + S(22);           /* four form rows, three rows of sequence buttons, the button row and the note line */
    fy = h - m - form;
    place(u->lv, x, y, W, fy - y - S(6) > S(80) ? fy - y - S(6) : S(80));
    lw = text_width(u->page, L"Player Side") + S(10);
    half = W / 2;
    place(u->lChar, x, fy + S(4), lw, S(18));  place(u->cbChar, x + lw, fy, W - lw, S(300));
    fy += S(28);
    place(u->lMove, x, fy + S(4), lw, S(18));  place(u->cbMove, x + lw, fy, W - lw, S(300));
    fy += S(28);
    place(u->lSeq, x, fy + S(4), lw, S(18));   place(u->eSeq, x + lw, fy, W - lw, rh);
    fy += S(28);
    {   /* three rows of ten buttons under the Sequence box */
        int k, gap = S(3), bw = (W - lw - 9 * gap) / 10, row;
        for (row = 0; row < 3; row++) {
            for (k = 0; k < 10; k++) if (u->tok[row * 10 + k]) place(u->tok[row * 10 + k], x + lw + k * (bw + gap), fy, bw, S(30));
            fy += S(32);
        }
    }
    place(u->lPlay, x, fy + S(4), text_width(u->page, L"Plays For") + S(10), S(18));
    place(u->cbPlay, x + text_width(u->page, L"Plays For") + S(10), fy, half - text_width(u->page, L"Plays For") - S(24), S(120));
    place(u->lFace, x + half, fy + S(4), lw, S(18));
    place(u->cbFace, x + half + lw, fy, W - half - lw, S(120));
    fy += S(30);
    place(u->bClear, x, fy, S(90), bh);
    place(u->bDef, x + W - S(90), fy, S(90), bh);
    place(u->note, x, fy + bh + S(4), W, S(18));
}

/* ---------------------------------------------------------------- the plugin for MAME */
static unsigned dir_mask_of(int d, int faceLeft) {
    static const char up[10] = {0,0,0,0,0,0,0,1,1,1}, down[10] = {0,1,1,1,0,0,0,0,0,0};
    static const char back[10] = {0,1,0,0,1,0,0,1,0,0}, fwd[10] = {0,0,0,1,0,0,1,0,0,1};
    unsigned m = 0;
    if (up[d]) m |= 1;
    if (down[d]) m |= 2;
    if (back[d]) m |= faceLeft ? 8 : 4;
    if (fwd[d]) m |= faceLeft ? 4 : 8;
    return m;
}

static int parse_button(const char *t, int len) {   /* 0..5 = Button 1..6, 6 = Start, -1 */
    static const char *NM[] = {"LP", "MP", "HP", "LK", "MK", "HK", 0};
    char b[8]; int i;
    if (len < 2 || len > 5) return -1;
    memcpy(b, t, (size_t)len); b[len] = 0;
    for (i = 0; NM[i]; i++) if (!_stricmp(NM[i], b)) return i;
    if (!_stricmp(b, "ST") || !_stricmp(b, "START")) return 6;
    if ((b[0] == 'B' || b[0] == 'b') && (b[1] == 'N' || b[1] == 'n') && b[2] >= '1' && b[2] <= '6' && !b[3]) return b[2] - '1';
    if ((b[0] == 'B' || b[0] == 'b') && b[1] >= '1' && b[1] <= '6' && !b[2]) return b[1] - '1';
    return -1;
}

typedef struct { unsigned mask; int ms; } Step;   /* mask: bit0 up, 1 down, 2 left, 3 right, 4..9 button 1..6, 10 start; ms 0 = the step length */

static void add_step(std::string &out, int &n, unsigned *lastMask, unsigned mask, int ms) {
    char t[40];
    if (n && (mask & *lastMask & 0x7F0)) { out += "{0,0},"; n++; }   /* the same button twice in a row needs a release between */
    snprintf(t, sizeof t, "{%u,%d},", mask, ms);
    out += t; n++;
    *lastMask = mask;
}

/* the sequence in the short notation as Lua steps; returns the number of steps */
static int seq_to_lua(const ComboSlot *c, std::string &out) {
    char buf[512], *tok, *ctx = 0;
    int n = 0;
    unsigned last = 0;
    strncpy(buf, c->seq, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (tok = strtok_s(buf, ", \t", &ctx); tok; tok = strtok_s(NULL, ", \t", &ctx)) {
        const char *p = tok;
        unsigned dirs[40], btn = 0;
        int nd = 0, k, hold = 0;
        if (!strcmp(tok, "~")) { add_step(out, n, &last, 0, 0); continue; }
        while (*p >= '1' && *p <= '9' && nd < 40) dirs[nd++] = dir_mask_of(*p++ - '0', c->faceLeft);
        if (nd > 0 && *p == '*') {
            int ms;
            p++;
            if (*p == 'C' || *p == 'c') { ms = g_in.chargeMs; p++; }
            else { ms = atoi(p); while (*p >= '0' && *p <= '9') p++; }
            hold = ms < 0 ? 0 : ms > 5000 ? 5000 : ms;
        }
        while (*p) {
            const char *e;
            int b;
            if (*p == '+') p++;
            e = p; while (*e && *e != '+') e++;
            b = parse_button(p, (int)(e - p));
            if (b >= 0) btn |= 1u << (4 + b);
            p = e;
        }
        if (nd == 0) { if (btn) add_step(out, n, &last, btn, 0); continue; }
        for (k = 0; k < nd - 1; k++) add_step(out, n, &last, dirs[k], 0);
        add_step(out, n, &last, dirs[nd - 1] | btn, hold);
    }
    return n;
}

static const char *LUA_BODY =
"local exports = {name = 'mamex_combos', version = '1.1.0', description = 'MAME EX combos', license = 'BSD-3-Clause', author = {name = 'MAME EX'}}\n"
"function exports.startplugin()\n"
"  local input, fields = nil, nil\n"
"  local macro = {}\n"
"  local applied = {{}, {}}\n"
"  local was = {}\n"
"  local seqs = {}\n"
"  local afseq, aft0 = {}, {}\n"
"  local logged = 0\n"
"  local function log(msg)\n"
"    if LOGPATH == '' or logged > 200 then return end\n"
"    logged = logged + 1\n"
"    local f = io.open(LOGPATH, 'a')\n"
"    if f then f:write(msg .. '\\n') f:close() end\n"
"  end\n"
"  local function now_ms() return manager.machine.time:as_double() * 1000 end\n"
"  -- every field of the two players: directions (bit 0-3), buttons 1-6 (bit 4-9), start (bit 10); a game can have several fields for one button (kick harness)\n"
"  local function find_fields()\n"
"    fields = {{}, {}}\n"
"    local n = 0\n"
"    for tag, port in pairs(manager.machine.ioport.ports) do\n"
"      for key, f in pairs(port.fields) do\n"
"        local nm = f.name or key\n"
"        local pl, what = nm:match('^P(%d) (.+)$')\n"
"        pl = tonumber(pl)\n"
"        if pl == 1 or pl == 2 then\n"
"          local bit = nil\n"
"          local dir = what:match('^Joystick (%a+)$') or what:match('^(%a+)$')\n"
"          if dir == 'Up' then bit = 0 elseif dir == 'Down' then bit = 1 elseif dir == 'Left' then bit = 2 elseif dir == 'Right' then bit = 3 end\n"
"          local b = what:match('Button (%d)$')\n"
"          if b and tonumber(b) >= 1 and tonumber(b) <= 6 then bit = 3 + tonumber(b) end\n"
"          if bit then\n"
"            fields[pl][bit] = fields[pl][bit] or {}\n"
"            local dup = false\n"
"            for _, g in ipairs(fields[pl][bit]) do if g == f then dup = true end end\n"
"            if not dup then table.insert(fields[pl][bit], f) n = n + 1 end\n"
"          end\n"
"        else\n"
"          local sp = nm:match('^(%d) Players? Start$')\n"
"          sp = tonumber(sp)\n"
"          if sp == 1 or sp == 2 then\n"
"            fields[sp][10] = fields[sp][10] or {}\n"
"            table.insert(fields[sp][10], f) n = n + 1\n"
"          end\n"
"        end\n"
"      end\n"
"    end\n"
"    log('fields found: ' .. n)\n"
"  end\n"
"  local function apply(pl, mask)\n"
"    for bit = 0, 10 do\n"
"      local list = fields[pl][bit]\n"
"      if list then\n"
"        local on = (mask >> bit) & 1 == 1\n"
"        if on then\n"
"          for _, f in ipairs(list) do f:set_value(1) end\n"
"          applied[pl][bit] = true\n"
"        elseif applied[pl][bit] then\n"
"          for _, f in ipairs(list) do f:clear_value() end\n"
"          applied[pl][bit] = false\n"
"        end\n"
"      end\n"
"    end\n"
"  end\n"
"  local function on_frame()\n"
"    if not input then input = manager.machine.input end\n"
"    if not fields then find_fields() end\n"
"    local now = now_ms()\n"
"    local af = {0, 0}\n"
"    for i, a in ipairs(AUTOFIRE) do   -- autofire: while the key / pad button of the row is held, its button is pressed every other half period\n"
"      if not afseq[i] then afseq[i] = input:seq_from_tokens(a.trig) end\n"
"      if input:seq_pressed(afseq[i]) then\n"
"        if not aft0[i] then aft0[i] = now end\n"
"        if math.floor((now - aft0[i]) / AF_HALF_MS) % 2 == 0 then af[a.player] = af[a.player] | (1 << a.bit) end\n"
"      else aft0[i] = nil end\n"
"    end\n"
"    for pl = 1, 2 do\n"
"      local m = macro[pl]\n"
"      local mask = 0\n"
"      if m then\n"
"        local len = m.c.steps[m.idx][2]\n"
"        if len == 0 then len = STEP_MS end\n"
"        if m.seen and now - m.t0 >= len then\n"
"          m.idx = m.idx + 1 m.t0 = now m.seen = false\n"
"          if m.idx > #m.c.steps then macro[pl] = nil m = nil log('combo done') end\n"
"        end\n"
"        if m then mask = m.c.steps[m.idx][1] m.seen = true end\n"
"      end\n"
"      apply(pl, mask | af[pl])\n"
"    end\n"
"    for i, c in ipairs(COMBOS) do\n"
"      if not seqs[i] then\n"
"        seqs[i] = input:seq_from_tokens(c.trig)\n"
"        log('combo ' .. i .. ' trigger ' .. c.trig .. ' = ' .. tostring(input:seq_name(seqs[i])))\n"
"      end\n"
"      local down = input:seq_pressed(seqs[i])\n"
"      if down and not was[i] and not macro[c.player] then\n"
"        macro[c.player] = {c = c, idx = 1, t0 = now, seen = false}\n"
"        log('combo ' .. i .. ' started')\n"
"      end\n"
"      was[i] = down\n"
"    end\n"
"  end\n"
"  local function safe_frame()\n"
"    local ok, err = pcall(on_frame)\n"
"    if not ok then log('error: ' .. tostring(err)) end\n"
"  end\n"
"  log('plugin started')\n"
"  if emu.add_machine_frame_notifier then exports.frame_sub = emu.add_machine_frame_notifier(safe_frame)   -- the subscription must be kept, or it is dropped\n"
"  elseif emu.register_frame then emu.register_frame(safe_frame)\n"
"  else log('no frame callback in this MAME') end\n"
"end\n"
"return exports\n";

/* writes the plugin; 1 when at least one combo has a trigger and steps */
int combos_plugin_prepare(wchar_t *pluginsPath, size_t cap) {
    wchar_t dir[560], sub[560], f[560];
    std::string lua, combos;
    int i, used = 0;
    std::string afl;
    int naf = bind_autofire_lua(afl);   /* the autofire rows of the Controls tab */
    if (!g_cfgLoaded) cmb_load();   /* started without the window (command line) */
    for (i = 0; i < NUM_COMBOS; i++) {
        const ComboSlot *c = &g_in.slots[i];
        std::string steps, trig;
        int n;
        if (!c->seq[0] || (!c->key[0] && !c->pad[0])) continue;
        n = seq_to_lua(c, steps);
        if (!n) continue;
        if (c->key[0]) trig += std::string("KEYCODE_") + c->key;
        if (c->pad[0]) { if (!trig.empty()) trig += " OR "; trig += std::string("JOYCODE_") + c->pad; }
        for (size_t q = 0; q < trig.size(); q++) if (trig[q] == '\'' || trig[q] == '\\' || trig[q] == '\n' || trig[q] == '\r') trig[q] = ' ';   /* the text goes into a Lua string */
        combos += "{player=" + std::string(c->player == 2 ? "2" : "1") + ",trig='" + trig + "',steps={" + steps + "}},\n";
        used++;
    }
    if (!used && !naf) return 0;
    plugin_path(dir, 560, L"mame-ex-combos-plugin");
    path_join(sub, 560, dir, L"mamex_combos");
    CreateDirectoryW(dir, NULL);
    CreateDirectoryW(sub, NULL);
    {
        wchar_t lp[560];
        char *lu;
        std::string logp;
        if (g_set.logs) log_path(lp, 560, L"mame-ex-combos.log"); else path_join(lp, 560, g_root, L"mame-ex-combos.log");   /* (no folder is made when logs are off) */
        if (g_set.logs) DeleteFileW(lp);
        lu = g_set.logs ? lua_path_u8(lp) : NULL;   /* without Enable Logs the plugin gets no log file (an empty path) */
        logp = lu ? lu : "";
        free(lu);
        for (size_t q = 0; q < logp.size(); q++) if (logp[q] == '\\') logp[q] = '/';
        lua = "-- written by MAME EX before every start: do not edit\nlocal STEP_MS = " + std::to_string(g_in.stepMs) + "\nlocal LOGPATH = [=[" + logp + "]=]\nlocal COMBOS = {\n" + combos + "}\nlocal AUTOFIRE = {\n" + afl + "}\nlocal AF_HALF_MS = " + std::to_string(500 / (g_set.afRate == 0 ? 10 : g_set.afRate == 2 ? 20 : g_set.afRate == 3 ? 30 : 15)) + "\n" + LUA_BODY;
    }
    path_join(f, 560, sub, L"init.lua");
    if (!write_file(f, lua.c_str(), lua.size())) return 0;
    {
        const char *js = "{\n\t\"plugin\": {\n\t\t\"name\": \"mamex_combos\",\n\t\t\"description\": \"MAME EX combos\",\n\t\t\"version\": \"1.0.0\",\n\t\t\"author\": \"MAME EX\",\n\t\t\"type\": \"plugin\",\n\t\t\"start\": \"false\"\n\t}\n}\n";
        path_join(f, 560, sub, L"plugin.json");
        if (!write_file(f, js, strlen(js))) return 0;
    }
    wcsncpy(pluginsPath, dir, cap - 1);
    pluginsPath[cap - 1] = 0;
    return 1;
}
