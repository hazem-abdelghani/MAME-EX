/* The part of bindings.cpp that does not touch Windows: the list of actions, reading and writing cfg\\default.cfg. (Included by bindings.cpp; tools/test_bindings.cpp tests it on any host.) */
#include <string>
#include <vector>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------- the actions */
static const struct { const char *token; const wchar_t *label; const char *defKey; int dir; } kRows[NBIND] = {
    {"P1_JOYSTICK_UP", L"Player 1 Up", "W", 1}, {"P1_JOYSTICK_DOWN", L"Player 1 Down", "S", 1}, {"P1_JOYSTICK_LEFT", L"Player 1 Left", "A", 1}, {"P1_JOYSTICK_RIGHT", L"Player 1 Right", "D", 1},
    {"P1_BUTTON1", L"Player 1 Button 1", "4PAD", 0}, {"P1_BUTTON2", L"Player 1 Button 2", "5PAD", 0}, {"P1_BUTTON3", L"Player 1 Button 3", "6PAD", 0},
    {"P1_BUTTON4", L"Player 1 Button 4", "1PAD", 0}, {"P1_BUTTON5", L"Player 1 Button 5", "2PAD", 0}, {"P1_BUTTON6", L"Player 1 Button 6", "3PAD", 0},
    {"P2_JOYSTICK_UP", L"Player 2 Up", "UP", 1}, {"P2_JOYSTICK_DOWN", L"Player 2 Down", "DOWN", 1}, {"P2_JOYSTICK_LEFT", L"Player 2 Left", "LEFT", 1}, {"P2_JOYSTICK_RIGHT", L"Player 2 Right", "RIGHT", 1},
    {"P2_BUTTON1", L"Player 2 Button 1", "U", 0}, {"P2_BUTTON2", L"Player 2 Button 2", "I", 0}, {"P2_BUTTON3", L"Player 2 Button 3", "O", 0},
    {"P2_BUTTON4", L"Player 2 Button 4", "J", 0}, {"P2_BUTTON5", L"Player 2 Button 5", "K", 0}, {"P2_BUTTON6", L"Player 2 Button 6", "L", 0},
    {"START1", L"Player 1 Start", "ENTER", 0}, {"START2", L"Player 2 Start", "Y", 0}, {"COIN1", L"Coin 1", "RSHIFT", 0}, {"COIN2", L"Coin 2", "H", 0},
    {"SERVICE", L"Service / Test", "F2", 0}};

/* the keys the launcher started with before these defaults (rows that still have them get the new ones) */
static const char *const kOldKey[NBIND] = {"UP", "DOWN", "LEFT", "RIGHT", "LCONTROL", "LALT", "SPACE", "LSHIFT", "Z", "X", "R", "F", "D", "G", "A", "S", "Q", "W", "E", "", "1", "2", "5", "6", "F2"};

typedef struct { char key[48]; char pad[72]; char extra[260]; int custom; int autofire; } Bind;   /* key / pad: the part after KEYCODE_ / JOYCODE_; extra: other codes of the sequence (mouse, gun ...) */
static Bind g_b[NBIND];
static int g_bindLoaded;

/* the buttons 1-6 of both players (the rows that can have autofire) */
int bind_is_button(int row) { return (row >= 4 && row <= 9) || (row >= 14 && row <= 19); }

/* Autofire: MAME does not press a button of an autofire row by itself (its entry in default.cfg is NONE); the real key / pad codes are kept here and
   given to the Lua plugin of the launcher, which presses the button over and over while they are held. */
static void af_file(wchar_t *out, size_t cap) { path_join(out, cap, g_root, L"mame-ex-autofire.cfg"); }

static void af_write(void) {
    wchar_t p[700];
    std::string t;
    int i;
    for (i = 0; i < NBIND; i++)
        if (g_b[i].custom && g_b[i].autofire && bind_is_button(i)) t += std::string(kRows[i].token) + "\t" + g_b[i].key + "\t" + g_b[i].pad + "\t" + g_b[i].extra + "\n";
    af_file(p, 700);
    if (t.empty()) { char *old = read_file(p, NULL); if (!old) return; free(old); }   /* nothing to write and no file: leave it */
    write_file(p, t.data(), t.size());
}

static void af_read(void) {
    wchar_t p[700];
    char *txt;
    std::string t;
    size_t pos = 0;
    af_file(p, 700);
    txt = read_file(p, NULL);
    if (!txt) return;
    t = txt;
    free(txt);
    while (pos < t.size()) {
        size_t e = t.find('\n', pos);
        std::string line = t.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        std::vector<std::string> f;
        size_t a = 0, q;
        int i;
        pos = e == std::string::npos ? t.size() : e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        while ((q = line.find('\t', a)) != std::string::npos) { f.push_back(line.substr(a, q - a)); a = q + 1; }
        f.push_back(line.substr(a));
        if (f.size() < 4) continue;
        for (i = 0; i < NBIND; i++) {
            if (f[0] != kRows[i].token || !bind_is_button(i)) continue;
            if (!g_b[i].custom || g_b[i].key[0] || g_b[i].pad[0] || g_b[i].extra[0]) continue;   /* the row is not NONE any more: changed outside the launcher */
            strncpy(g_b[i].key, f[1].c_str(), sizeof g_b[i].key - 1);
            strncpy(g_b[i].pad, f[2].c_str(), sizeof g_b[i].pad - 1);
            strncpy(g_b[i].extra, f[3].c_str(), sizeof g_b[i].extra - 1);
            g_b[i].autofire = 1;
        }
    }
}
static int g_dirty;

const wchar_t *bind_label(int row) { return kRows[row].label; }
int bind_dirty(void) { return g_dirty; }

/* ---------------------------------------------------------------- default.cfg */
static void cfg_file(wchar_t *out, size_t cap) {
    wchar_t md[560], dir[600];
    mame_dir(md, 560);
    path_join(dir, 600, md, L"cfg");
    path_join(out, cap, dir, L"default.cfg");
}

static std::string trim_s(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

/* the sequence "KEYCODE_A OR JOYCODE_1_BUTTON1 OR MOUSECODE_1_BUTTON1" into the key, the pad button and the rest */
static void parse_seq(const std::string &seq, Bind *b) {
    size_t pos = 0;
    memset(b, 0, sizeof *b);
    b->custom = 1;
    if (trim_s(seq) == "NONE") return;
    {   /* the alternatives (separated by OR); one with several codes (KEYCODE_LALT KEYCODE_ENTER = both, KEYCODE_A NOT KEYCODE_LSHIFT) is kept as it is in "extra" */
        std::vector<std::string> alts;
        std::string cur;
        while (pos < seq.size()) {
            size_t e = seq.find_first_of(" \t\r\n", pos);
            std::string tok = seq.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
            pos = e == std::string::npos ? seq.size() : e + 1;
            if (tok.empty()) continue;
            if (tok == "OR") { if (!cur.empty()) alts.push_back(cur); cur.clear(); continue; }
            if (!cur.empty()) cur += " ";
            cur += tok;
        }
        if (!cur.empty()) alts.push_back(cur);
        for (size_t k = 0; k < alts.size(); k++) {
            const std::string &s = alts[k];
            bool single = s.find(' ') == std::string::npos;
            if (single && !strncmp(s.c_str(), "KEYCODE_", 8) && !b->key[0] && s.size() < 47) strcpy(b->key, s.c_str() + 8);
            else if (single && !strncmp(s.c_str(), "JOYCODE_", 8) && !b->pad[0] && s.size() < 79) strcpy(b->pad, s.c_str() + 8);
            else {
                size_t used = strlen(b->extra), need = s.size() + (used ? 4 : 0);
                if (used + need >= sizeof b->extra) continue;   /* it does not fit: left out whole (never cut in the middle of a code) */
                if (used) strcat(b->extra, " OR ");
                strcat(b->extra, s.c_str());
            }
        }
    }
}

/* the part of the file from "<port type=\"TOKEN\"" to its "</port>" (and the line start before it): from, to; 0 when there is none */
static int find_port(const std::string &t, const char *token, size_t *from, size_t *to) {
    std::string key = std::string("<port type=\"") + token + "\"";
    size_t a = t.find(key), e;
    if (a == std::string::npos) return 0;
    e = t.find("</port>", a);
    if (e == std::string::npos) return 0;
    *to = e + 7;
    while (a > 0 && (t[a - 1] == ' ' || t[a - 1] == '\t')) a--;
    *from = a;
    return 1;
}

void bind_load(void) {
    wchar_t p[700];
    char *txt;
    std::string t;
    int i;
    memset(g_b, 0, sizeof g_b);
    g_dirty = 0;
    g_bindLoaded = 1;
    cfg_file(p, 700);
    txt = read_file(p, NULL);
    if (!txt) return;
    t = txt;
    free(txt);
    for (i = 0; i < NBIND; i++) {
        size_t a, e, s0, s1, v0;
        if (!find_port(t, kRows[i].token, &a, &e)) continue;
        std::string blk = t.substr(a, e - a);
        s0 = blk.find("<newseq type=\"standard\">");
        if (s0 == std::string::npos) continue;
        v0 = s0 + strlen("<newseq type=\"standard\">");
        s1 = blk.find("</newseq>", v0);
        if (s1 == std::string::npos) continue;
        parse_seq(blk.substr(v0, s1 - v0), &g_b[i]);
    }
    af_read();
}

static int g_afNative;   /* 1: the autofire rows are written with their real codes (no plugin will press them) */
static std::string make_port(int i) {
    const Bind *b = &g_b[i];
    std::string seq;
    if (b->autofire && bind_is_button(i) && !g_afNative) return std::string("            <port type=\"") + kRows[i].token + "\">\n                <newseq type=\"standard\">\n                    NONE\n                </newseq>\n            </port>\n";   /* the plugin presses it */
    if (b->key[0]) seq += std::string("KEYCODE_") + b->key;
    if (b->pad[0]) { if (!seq.empty()) seq += " OR "; seq += std::string("JOYCODE_") + b->pad; }
    if (b->extra[0]) { if (!seq.empty()) seq += " OR "; seq += b->extra; }
    if (seq.empty()) seq = "NONE";
    return std::string("            <port type=\"") + kRows[i].token + "\">\n                <newseq type=\"standard\">\n                    " + seq + "\n                </newseq>\n            </port>\n";
}

int bind_save(void);
/* at the start of a game: with the plugin the autofire buttons are NONE in MAME's file (the plugin presses them), without it (the filter is off for the
   game, or the plugin could not be written) they get their real keys back, so the button at least works, without the repeat */
int bind_autofire_mode(int pluginRuns) {
    int i, any = 0, r;
    for (i = 0; i < NBIND; i++) if (g_b[i].custom && g_b[i].autofire && bind_is_button(i)) any = 1;
    if (!any) return 1;
    g_afNative = !pluginRuns;
    g_dirty = 1;
    r = bind_save();
    g_afNative = 0;
    return r;
}

int bind_save(void) {
    wchar_t p[700], dir[700];
    char *txt;
    std::string t, add, res;
    int i, any = 0;
    size_t pos;
    if (!g_dirty) return 1;
    af_write();
    cfg_file(p, 700);
    wcscpy(dir, p);
    { wchar_t *s = wcsrchr(dir, L'\\'); if (s) { *s = 0; CreateDirectoryW(dir, NULL); } }
    txt = read_file(p, NULL);
    if (txt) { t = txt; free(txt); }
    for (i = 0; i < NBIND; i++) {   /* the old entries of our ports go; the rest of the file stays */
        size_t a, e;
        while (find_port(t, kRows[i].token, &a, &e)) {
            if (e < t.size() && t[e] == '\r') e++;
            if (e < t.size() && t[e] == '\n') e++;
            t.erase(a, e - a);
        }
        if (g_b[i].custom) { add += make_port(i); any = 1; }
    }
    if (t.empty() || t.find("<mameconfig") == std::string::npos) {   /* no file (or none we know): a new one */
        if (!any) return 1;
        res = "<?xml version=\"1.0\"?>\n<mameconfig version=\"10\">\n    <system name=\"default\">\n        <input>\n" + add + "        </input>\n    </system>\n</mameconfig>\n";
    } else {
        size_t sys = t.find("<system name=\"default\">"), in, inEnd;
        res = t;
        if (sys == std::string::npos) {   /* a file without the default system: ours is added */
            if (!any) { g_dirty = 0; return 1; }
            pos = res.rfind("</mameconfig>");
            if (pos == std::string::npos) return 0;
            res.insert(pos, "    <system name=\"default\">\n        <input>\n" + add + "        </input>\n    </system>\n");
        } else {
            in = res.find("<input>", sys);
            inEnd = res.find("</system>", sys);
            if (in != std::string::npos && inEnd != std::string::npos && in < inEnd) {   /* the input part of the default system */
                pos = res.find("</input>", in);
                if (pos == std::string::npos || pos > inEnd) return 0;
                while (pos > 0 && (res[pos - 1] == ' ' || res[pos - 1] == '\t')) pos--;
                res.insert(pos, add);
            } else {
                if (inEnd == std::string::npos) return 0;
                if (!any) { g_dirty = 0; return 1; }
                res.insert(inEnd, "        <input>\n" + add + "        </input>\n    ");
            }
        }
    }
    if (!write_file(p, res.data(), res.size())) return 0;
    g_dirty = 0;
    return 1;
}

/* every pad code of a row: the pad column and the JOYCODE_ words of the rest (a direction can have the D-pad and the stick) */
void bind_pad_list(const Bind *b, std::vector<std::string> &out) {
    size_t pos = 0;
    std::string x = b->extra;
    if (b->pad[0]) out.push_back(b->pad);
    while (pos < x.size()) {
        size_t e = x.find(" OR ", pos);
        std::string tok = x.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        pos = e == std::string::npos ? x.size() : e + 4;
        if (!strncmp(tok.c_str(), "JOYCODE_", 8)) out.push_back(tok.substr(8));
    }
}

/* the codes of a row back into the pad column and the rest (the words that are not pad codes stay) */
static void bind_pad_store(Bind *b, const std::vector<std::string> &codes) {
    size_t pos = 0, i;
    std::string x = b->extra, keep;
    while (pos < x.size()) {
        size_t e = x.find(" OR ", pos);
        std::string tok = x.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        pos = e == std::string::npos ? x.size() : e + 4;
        if (strncmp(tok.c_str(), "JOYCODE_", 8)) { if (!keep.empty()) keep += " OR "; keep += tok; }
    }
    b->pad[0] = 0;
    for (i = 0; i < codes.size(); i++) {
        if (i == 0) { strncpy(b->pad, codes[i].c_str(), sizeof b->pad - 1); continue; }
        if (!keep.empty()) keep += " OR ";
        keep += "JOYCODE_" + codes[i];
    }
    strncpy(b->extra, keep.c_str(), sizeof b->extra - 1);
    b->extra[sizeof b->extra - 1] = 0;
}

/* what the launcher assumes about the pads (bindings.cpp asks MAME about the pads of this PC; the test uses plain stand-ins) */
int bind_pad_dev(int pl);                         /* the JOYCODE number of the pad of player pl */
int bind_pad_xinput(int dev);                     /* 1 when that device is an XInput pad (or nothing is known about it) */
std::string bind_trigger_code(int dev, int right);   /* the code of the LT / RT trigger of that pad, as MAME names it */

/* the default pad codes of an action; buttons: 1 X, 2 Y, 3 RB, 4 A, 5 B, 6 RT (the Xbox names; on a DirectInput pad the same positions: buttons 1 2 3 4 and 6, the trigger as the Z axis) */
static void default_pad(int row, std::vector<std::string> &codes) {
    int pl = (row < 10 || row == 20 || row == 22) ? 1 : 2, dev = bind_pad_dev(pl);
    char t[96];
    if (kRows[row].dir) {
        static const char *hat[4] = {"HAT1UP", "HAT1DOWN", "HAT1LEFT", "HAT1RIGHT"}, *stick[4] = {"YAXIS_UP_SWITCH", "YAXIS_DOWN_SWITCH", "XAXIS_LEFT_SWITCH", "XAXIS_RIGHT_SWITCH"};
        int d = row % 10;
        snprintf(t, sizeof t, "%d_%s", dev, hat[d]); codes.push_back(t);
        snprintf(t, sizeof t, "%d_%s", dev, stick[d]); codes.push_back(t);
    } else if (row == 20 || row == 21) { snprintf(t, sizeof t, bind_pad_xinput(dev) ? "%d_START" : "%d_BUTTON8", dev); codes.push_back(t); }
    else if (row == 22 || row == 23) { snprintf(t, sizeof t, bind_pad_xinput(dev) ? "%d_SELECT" : "%d_BUTTON7", dev); codes.push_back(t); }
    else if (row < 20) {   /* rows 4-9 and 14-19: buttons 1-6 */
        static const int num[7] = {0, 3, 4, 6, 1, 2, 0};   /* buttons 1-6: X, Y, RB, A, B, RT */
        int k = row % 10 - 3;
        if (k == 6) codes.push_back(bind_trigger_code(dev, 1));
        else { snprintf(t, sizeof t, "%d_BUTTON%d", dev, num[k]); codes.push_back(t); }
    }
}

/* a row that was MAME's default becomes ours: it starts with the default key and the default pad codes (the D-pad and the left stick for the directions,
   the buttons of an Xbox pad), so changing the key does not take the pad away, nor the other way round */
static void make_custom(int row) {
    Bind *b = &g_b[row];
    std::vector<std::string> codes;
    if (b->custom) return;
    memset(b, 0, sizeof *b);
    b->custom = 1;
    strncpy(b->key, kRows[row].defKey, sizeof b->key - 1);
    default_pad(row, codes);
    bind_pad_store(b, codes);
}

void bind_set_key(int row, const char *key) {
    make_custom(row);
    strncpy(g_b[row].key, key, sizeof g_b[row].key - 1);
    g_b[row].key[sizeof g_b[row].key - 1] = 0;
    g_dirty = 1;
}

/* a pad code from the capture window: for a direction it replaces the D-pad code (or the stick code) and keeps the other one, so both work;
   a button replaces the pad code; "" removes the pad codes */
void bind_set_pad(int row, const char *code) {
    std::vector<std::string> codes, out;
    size_t i;
    make_custom(row);
    if (!code[0]) { bind_pad_store(&g_b[row], out); g_dirty = 1; return; }
    if (kRows[row].dir) {
        bool hat = strstr(code, "HAT") != NULL;
        bind_pad_list(&g_b[row], codes);
        for (i = 0; i < codes.size(); i++) if ((strstr(codes[i].c_str(), "HAT") != NULL) != hat) out.push_back(codes[i]);
        out.insert(out.begin(), code);
    } else out.push_back(code);
    bind_pad_store(&g_b[row], out);
    g_dirty = 1;
}

void bind_clear(int row, int col) {
    if (col == 1) { make_custom(row); g_b[row].key[0] = 0; g_dirty = 1; }
    else bind_set_pad(row, "");
}

/* the first start of the launcher: MAME's cfg\\default.cfg is kept once as default.cfg.mame-ex-backup before the defaults are written into it */
void bind_backup_cfg(void) {
    wchar_t p[700], bak[720];
    cfg_file(p, 700);
    _snwprintf(bak, 720, L"%ls.mame-ex-backup", p);
    bak[719] = 0;
    char *old = read_file(bak, NULL), *cur;
    size_t n = 0;
    if (old) { free(old); return; }   /* an earlier backup is never replaced */
    cur = read_file(p, &n);
    if (cur) { write_file(bak, cur, n); free(cur); }
}

void bind_reset_all(void) {   /* the launcher's defaults: every action written with its key and pad codes */
    int i;
    memset(g_b, 0, sizeof g_b);
    for (i = 0; i < NBIND; i++) make_custom(i);
    g_dirty = 1;
}


/* the mapping as text for a profile: one line per changed action: bind=TOKEN <tab> KEY <tab> PAD <tab> OTHER */
void bind_to_text(std::string &out) {
    int i;
    for (i = 0; i < NBIND; i++)
        if (g_b[i].custom) out += std::string("bind=") + kRows[i].token + "\t" + g_b[i].key + "\t" + g_b[i].pad + "\t" + g_b[i].extra + "\t" + (g_b[i].autofire ? "1" : "0") + "\n";
}

/* the mapping of a profile: the actions it does not mention go back to MAME's defaults; written to the file with the next OK */
void bind_from_text(const char *text) {
    const char *p = text;
    memset(g_b, 0, sizeof g_b);
    while (p && *p) {
        const char *e = strchr(p, '\n');
        std::string line(p, e ? (size_t)(e - p) : strlen(p));
        p = e ? e + 1 : NULL;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, 5, "bind=") != 0) continue;
        {
            std::vector<std::string> f;
            size_t a = 5, t;
            int i;
            while ((t = line.find('\t', a)) != std::string::npos) { f.push_back(line.substr(a, t - a)); a = t + 1; }
            f.push_back(line.substr(a));
            if (f.size() < 4) continue;
            for (i = 0; i < NBIND; i++) if (f[0] == kRows[i].token) {
                memset(&g_b[i], 0, sizeof g_b[i]);
                g_b[i].custom = 1;
                g_b[i].autofire = f.size() > 4 && f[4] == "1" && bind_is_button(i);
                strncpy(g_b[i].key, f[1].c_str(), sizeof g_b[i].key - 1);
                strncpy(g_b[i].pad, f[2].c_str(), sizeof g_b[i].pad - 1);
                strncpy(g_b[i].extra, f[3].c_str(), sizeof g_b[i].extra - 1);
            }
        }
    }
    g_dirty = 1;
}

void bind_set_autofire(int row, int on) {
    if (!bind_is_button(row)) return;
    make_custom(row);
    g_b[row].autofire = on ? 1 : 0;
    g_dirty = 1;
}
int bind_get_autofire(int row) { return g_b[row].custom && g_b[row].autofire; }

/* the autofire rows as the entries of a Lua table: {player=1,bit=4,trig='KEYCODE_X OR JOYCODE_1_BUTTON3'}, ... ; the number of entries */
int bind_autofire_lua(std::string &out) {
    int i, n = 0;
    if (!g_bindLoaded) bind_load();
    for (i = 0; i < NBIND; i++) {
        const Bind *b = &g_b[i];
        std::string trig;
        char t[40];
        if (!b->custom || !b->autofire || !bind_is_button(i)) continue;
        if (b->key[0]) trig += std::string("KEYCODE_") + b->key;
        if (b->pad[0]) { if (!trig.empty()) trig += " OR "; trig += std::string("JOYCODE_") + b->pad; }
        if (b->extra[0]) { if (!trig.empty()) trig += " OR "; trig += b->extra; }
        if (trig.empty()) continue;
        for (size_t q = 0; q < trig.size(); q++) if (trig[q] == '\'' || trig[q] == '\\' || trig[q] == '\n' || trig[q] == '\r') trig[q] = ' ';   /* the text goes into a Lua string */
        snprintf(t, sizeof t, "{player=%d,bit=%d,trig='", i < 10 ? 1 : 2, i % 10);
        out += t + trig + "'},\n";
        n++;
    }
    return n;
}
