// Tests the cfg\default.cfg reading / writing of the key mapping on any host:
//   g++ -std=c++17 -I src tools/test_bindings.cpp -o /tmp/test_bindings && /tmp/test_bindings
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#define NBIND 25
#define _snwprintf swprintf
#define CreateDirectoryW(a, b) ((void)(a), 0)
static std::string g_file;
static bool g_exists;
static std::string g_af;
static bool g_afExists;
wchar_t g_root[560] = L"r";
char *read_file(const wchar_t *p, size_t *len) {
    if (wcsstr(p, L"autofire")) {
        if (!g_afExists) return nullptr;
        char *q = (char *)malloc(g_af.size() + 1);
        memcpy(q, g_af.data(), g_af.size()); q[g_af.size()] = 0;
        if (len) *len = g_af.size();
        return q;
    }
    if (!g_exists) return nullptr;
    char *r = (char *)malloc(g_file.size() + 1);
    memcpy(r, g_file.data(), g_file.size());
    r[g_file.size()] = 0;
    if (len) *len = g_file.size();
    return r;
}
int write_file(const wchar_t *p, const void *d, size_t n) { if (wcsstr(p, L"autofire")) { g_af.assign((const char *)d, n); g_afExists = true; return 1; } g_file.assign((const char *)d, n); g_exists = true; return 1; }
void mame_dir(wchar_t *out, size_t cap) { wcsncpy(out, L"m", cap); }
void path_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b) { swprintf(out, cap, L"%ls\\%ls", a, b); }
int bind_pad_dev(int pl) { return pl; }
int bind_pad_xinput(int) { return 1; }
std::string bind_trigger_code(int dev, int right) { return std::to_string(dev) + (right ? "_SLIDER2_NEG_SWITCH" : "_SLIDER1_NEG_SWITCH"); }
#include "bind_core.inc"
static struct { int ovGameCfg; } g_set = {1};
int padprobe_token(int, const char *, char *, size_t, int *) { return 0; }
int padprobe_is_xinput(int) { return 1; }
int file_exists(const wchar_t *) { return 0; }
#include "bind_migrate.inc"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int count(const std::string &s, const std::string &sub) { int n = 0; size_t p = 0; while ((p = s.find(sub, p)) != std::string::npos) { n++; p += sub.size(); } return n; }
static void setrow(int i, const char *key, const char *pad) { memset(&g_b[i], 0, sizeof g_b[i]); g_b[i].custom = 1; strcpy(g_b[i].key, key); strcpy(g_b[i].pad, pad); g_dirty = 1; }

int main() {
    // 1. no file yet: a new one
    g_exists = false; bind_load();
    for (int i = 0; i < NBIND; i++) CHECK(!g_b[i].custom);
    setrow(4, "LCONTROL", "1_BUTTON1"); setrow(0, "UP", "1_HAT1UP");
    CHECK(bind_save());
    CHECK(g_file.find("<mameconfig version=\"10\">") != std::string::npos);
    CHECK(count(g_file, "<port type=\"P1_BUTTON1\">") == 1);
    CHECK(g_file.find("KEYCODE_LCONTROL OR JOYCODE_1_BUTTON1") != std::string::npos);
    bind_load();
    CHECK(g_b[4].custom && !strcmp(g_b[4].key, "LCONTROL") && !strcmp(g_b[4].pad, "1_BUTTON1"));
    CHECK(g_b[0].custom && !strcmp(g_b[0].pad, "1_HAT1UP") && !g_b[1].custom);

    // 2. a file as MAME writes it, with other entries that must stay
    g_file =
        "<?xml version=\"1.0\"?>\n<!-- comment -->\n<mameconfig version=\"10\">\n    <system name=\"default\">\n        <input>\n"
        "            <port type=\"UI_UP\">\n                <newseq type=\"standard\">\n                    KEYCODE_UP OR JOYCODE_1_HAT1UP\n                </newseq>\n            </port>\n"
        "            <port type=\"P1_BUTTON1\">\n                <newseq type=\"standard\">\n                    KEYCODE_A OR MOUSECODE_1_BUTTON1\n                </newseq>\n            </port>\n"
        "            <port type=\"P1_BUTTON2\">\n                <newseq type=\"standard\">\n                    NONE\n                </newseq>\n            </port>\n"
        "        </input>\n    </system>\n</mameconfig>\n";
    g_exists = true; bind_load();
    CHECK(g_b[4].custom && !strcmp(g_b[4].key, "A") && !g_b[4].pad[0] && !strcmp(g_b[4].extra, "MOUSECODE_1_BUTTON1"));
    CHECK(g_b[5].custom && !g_b[5].key[0] && !g_b[5].pad[0]);
    // AND / NOT sequences made in MAME's own menu stay as they are
    std::string keepFile = g_file;
    g_file = "<mameconfig version=\"10\"><system name=\"default\"><input>\n<port type=\"P1_BUTTON1\">\n<newseq type=\"standard\">\nKEYCODE_LALT KEYCODE_ENTER OR KEYCODE_A NOT KEYCODE_LSHIFT OR JOYCODE_1_BUTTON1\n</newseq>\n</port>\n</input></system></mameconfig>\n";
    bind_load();
    CHECK(!strcmp(g_b[4].extra, "KEYCODE_LALT KEYCODE_ENTER OR KEYCODE_A NOT KEYCODE_LSHIFT") && !strcmp(g_b[4].pad, "1_BUTTON1") && !g_b[4].key[0]);
    g_file = keepFile; bind_load();
    setrow(6, "SPACE", "2_BUTTON3"); strcpy(g_b[4].pad, "1_BUTTON1"); g_dirty = 1;
    CHECK(bind_save());
    CHECK(count(g_file, "<port type=\"UI_UP\">") == 1);                       // others stay
    CHECK(count(g_file, "<port type=\"P1_BUTTON1\">") == 1);                  // not duplicated
    CHECK(g_file.find("KEYCODE_A OR JOYCODE_1_BUTTON1 OR MOUSECODE_1_BUTTON1") != std::string::npos);
    CHECK(g_file.find("<!-- comment -->") != std::string::npos);
    CHECK(count(g_file, "</mameconfig>") == 1 && count(g_file, "<input>") == 1 && count(g_file, "</input>") == 1);
    bind_load();
    CHECK(!strcmp(g_b[6].pad, "2_BUTTON3") && g_b[5].custom);

    // 3. the defaults of the launcher: every action written with its key and pad codes, the other entries stay
    bind_reset_all();
    CHECK(!strcmp(g_b[0].key, "W") && !strcmp(g_b[4].key, "4PAD") && !strcmp(g_b[9].key, "3PAD") && !strcmp(g_b[10].key, "UP") && !strcmp(g_b[20].key, "ENTER") && !strcmp(g_b[22].key, "RSHIFT") && !strcmp(g_b[23].key, "H") && !strcmp(g_b[19].key, "L"));
    CHECK(!strcmp(g_b[4].pad, "1_BUTTON3") && !strcmp(g_b[5].pad, "1_BUTTON4") && !strcmp(g_b[6].pad, "1_BUTTON6") && !strcmp(g_b[7].pad, "1_BUTTON1") && !strcmp(g_b[8].pad, "1_BUTTON2") && !strcmp(g_b[9].pad, "1_SLIDER2_NEG_SWITCH"));
    CHECK(!strcmp(g_b[19].pad, "2_SLIDER2_NEG_SWITCH") && !strcmp(g_b[20].pad, "1_START") && !strcmp(g_b[22].pad, "1_SELECT"));
    CHECK(bind_save());
    CHECK(count(g_file, "<port type=\"P1_BUTTON1\">") == 1 && count(g_file, "<port type=\"UI_UP\">") == 1);
    bind_load();
    for (int i = 0; i < NBIND; i++) CHECK(g_b[i].custom);
    memset(g_b, 0, sizeof g_b);   // (nothing custom from here on)
    g_dirty = 1;
    CHECK(bind_save());
    CHECK(count(g_file, "<port type=\"P1_BUTTON") == 0);
    bind_load();
    for (int i = 0; i < NBIND; i++) CHECK(!g_b[i].custom);

    // 4. clear one column of a default row
    bind_clear(20, 2); // START1: pad column cleared -> custom, the default key stays, no pad
    CHECK(g_b[20].custom && !strcmp(g_b[20].key, "ENTER") && !g_b[20].pad[0]);
    bind_clear(20, 1); // and the key too: nothing left
    CHECK(!g_b[20].key[0] && !g_b[20].pad[0]);
    CHECK(bind_save() && g_file.find("<port type=\"START1\">") != std::string::npos && g_file.find("NONE") != std::string::npos);

    // 4b. changing the key of a default row keeps the pad (D-pad + stick), the pad of a direction keeps the other kind
    memset(g_b, 0, sizeof g_b);
    bind_set_key(0, "W");   // P1 up
    CHECK(!strcmp(g_b[0].key, "W") && !strcmp(g_b[0].pad, "1_HAT1UP") && !strcmp(g_b[0].extra, "JOYCODE_1_YAXIS_UP_SWITCH"));
    bind_set_pad(0, "1_YAXIS_UP_SWITCH");   // the stick again: D-pad stays
    { std::vector<std::string> v; bind_pad_list(&g_b[0], v); CHECK(v.size() == 2 && v[0] == "1_YAXIS_UP_SWITCH" && v[1] == "1_HAT1UP"); }
    bind_set_pad(0, "2_HAT1UP");   // another D-pad replaces the D-pad only
    { std::vector<std::string> v; bind_pad_list(&g_b[0], v); CHECK(v.size() == 2 && v[0] == "2_HAT1UP" && v[1] == "1_YAXIS_UP_SWITCH"); }
    bind_set_key(4, "Q");   // P1 button 1: key changed, pad = A
    CHECK(!strcmp(g_b[4].pad, "1_BUTTON3"));
    bind_set_pad(4, "1_BUTTON3");
    CHECK(!strcmp(g_b[4].pad, "1_BUTTON3") && !g_b[4].extra[0]);
    bind_set_key(19, "T");  // P2 button 6
    CHECK(!strcmp(g_b[19].pad, "2_SLIDER2_NEG_SWITCH"));
    bind_set_key(23, "7");  // COIN2
    CHECK(!strcmp(g_b[23].pad, "2_SELECT"));
    memset(g_b, 0, sizeof g_b);

    // 5. a default system without input part
    g_file = "<?xml version=\"1.0\"?>\n<mameconfig version=\"10\">\n    <system name=\"default\">\n    </system>\n</mameconfig>\n";
    memset(g_b, 0, sizeof g_b); setrow(22, "5", ""); 
    CHECK(bind_save() && count(g_file, "<input>") == 1 && g_file.find("<port type=\"COIN1\">") != std::string::npos);
    // 6. nothing custom, nothing in the file: the file is left alone
    g_file = "<mameconfig version=\"10\"><system name=\"x\"/></mameconfig>"; std::string keep = g_file;
    memset(g_b, 0, sizeof g_b); g_dirty = 1;
    CHECK(bind_save() && g_file == keep);

    printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
    { /* profile text round trip */
        std::string t, t2;
        memset(g_b, 0, sizeof g_b);
        strcpy(g_b[0].key, "KEYCODE_Q"); g_b[0].custom = 1;
        strcpy(g_b[3].pad, "JOYCODE_1_BUTTON2"); g_b[3].custom = 1;
        bind_to_text(t);
        memset(g_b, 0, sizeof g_b);
        bind_from_text(t.c_str());
        bind_to_text(t2);
        CHECK(t == t2 && !strcmp(g_b[0].key, "KEYCODE_Q") && !strcmp(g_b[3].pad, "JOYCODE_1_BUTTON2") && !g_b[1].custom);
    }
    { /* the repair of rows of older builds */
        memset(g_b, 0, sizeof g_b);
        setrow(4, "", "1_BUTTON3");            // P1 button 1: pad only (older build)
        setrow(0, "", "1_YAXIS_UP_SWITCH");    // P1 up: stick only
        setrow(9, "", "1_RZAXIS_NEG_SWITCH");  // P1 button 6: the first RT try
        setrow(20, "", "1_START");             // start 1
        setrow(21, "", "");                    // start 2: cleared on purpose
        CHECK(bind_migrate());
        CHECK(!strcmp(g_b[4].key, "4PAD") && !strcmp(g_b[4].pad, "1_BUTTON3"));
        { std::vector<std::string> v; bind_pad_list(&g_b[0], v); CHECK(!strcmp(g_b[0].key, "W") && v.size() == 2 && v[0] == "1_YAXIS_UP_SWITCH" && v[1] == "1_HAT1UP"); }
        CHECK(!strcmp(g_b[9].pad, "1_SLIDER2_NEG_SWITCH") && !strcmp(g_b[9].key, "3PAD"));
        CHECK(!strcmp(g_b[20].key, "ENTER") && !g_b[21].key[0] && !g_b[21].pad[0]);
        CHECK(!bind_migrate());                // nothing left to repair
        memset(g_b, 0, sizeof g_b);            // the first pad defaults (A B X Y LB RB) become A B RT X Y RB
        for (int k = 1; k <= 6; k++) { char p[24]; snprintf(p, sizeof p, "1_BUTTON%d", k); setrow(3 + k, kRows[3 + k].defKey, p); }
        CHECK(bind_migrate());
        CHECK(!strcmp(g_b[4].pad, "1_BUTTON3") && !strcmp(g_b[5].pad, "1_BUTTON4") && !strcmp(g_b[6].pad, "1_BUTTON6") && !strcmp(g_b[7].pad, "1_BUTTON1") && !strcmp(g_b[8].pad, "1_BUTTON2") && !strcmp(g_b[9].pad, "1_SLIDER2_NEG_SWITCH"));
        memset(g_b, 0, sizeof g_b);            // the old default keys become the new ones, rows that were MAME's own get the defaults
        strcpy(g_b[0].key, "UP"); g_b[0].custom = 1; strcpy(g_b[1].key, "Q"); g_b[1].custom = 1;
        CHECK(bind_migrate());
        CHECK(!strcmp(g_b[0].key, "W") && !strcmp(g_b[1].key, "Q") && g_b[2].custom && !strcmp(g_b[2].key, "A") && !strcmp(g_b[2].pad, "1_HAT1LEFT"));
    }
    { /* a game's own cfg: the mapped ports go, the rest stays */
        g_file = "<?xml version=\"1.0\"?>\n<mameconfig version=\"10\">\n    <system name=\"x\">\n        <input>\n            <port tag=\":P1\" type=\"P1_BUTTON1\" mask=\"16\" defvalue=\"16\">\n                <newseq type=\"standard\">\n                    JOYCODE_1_BUTTON3\n                </newseq>\n            </port>\n            <port type=\"COIN1\"><newseq type=\"standard\">KEYCODE_5</newseq></port>\n            <port type=\"P1_PEDAL\"><newseq type=\"standard\">KEYCODE_Z</newseq></port>\n            <port type=\"CLASS\" tag=\":DSW\" mask=\"3\" defvalue=\"3\" value=\"1\" />\n        </input>\n    </system>\n</mameconfig>\n";
        g_exists = true;
        bind_strip_game_cfg(L"x");
        CHECK(g_file.find("P1_BUTTON1") == std::string::npos && g_file.find("COIN1") == std::string::npos);
        CHECK(g_file.find("P1_PEDAL") != std::string::npos && g_file.find(":DSW") != std::string::npos);
    }
    { /* autofire: the row is NONE for MAME, the codes are kept in the autofire file and come back */
        memset(g_b, 0, sizeof g_b);
        g_file.clear(); g_exists = false; g_af.clear(); g_afExists = false;
        bind_reset_all();
        bind_set_autofire(4, 1);
        bind_set_autofire(0, 1);   // not a button: ignored
        CHECK(bind_get_autofire(4) && !bind_get_autofire(0));
        CHECK(bind_save());
        CHECK(g_file.find("NONE") != std::string::npos && g_af.find("P1_BUTTON1\t4PAD\t1_BUTTON3") != std::string::npos);
        bind_load();
        CHECK(bind_get_autofire(4) && !strcmp(g_b[4].key, "4PAD") && !strcmp(g_b[4].pad, "1_BUTTON3") && !bind_get_autofire(5));
        { std::string lua; CHECK(bind_autofire_lua(lua) == 1 && lua.find("{player=1,bit=4,trig='KEYCODE_4PAD OR JOYCODE_1_BUTTON3'}") != std::string::npos); }
        bind_set_autofire(4, 0);
        CHECK(bind_save() && g_file.find("KEYCODE_4PAD OR JOYCODE_1_BUTTON3") != std::string::npos && g_af.empty());
        { std::string tx; bind_set_autofire(14, 1); bind_to_text(tx); memset(g_b, 0, sizeof g_b); bind_from_text(tx.c_str()); CHECK(bind_get_autofire(14) && !bind_get_autofire(4)); }
    }
    return fails != 0;
}
