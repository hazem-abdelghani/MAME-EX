/* What MAME knows about one game besides its name: the BIOS versions and the DIP switches (from `mame -listxml <set>`), used by Game > Game Settings:
   a BIOS box (the -bios option of the game) and a DIP Switches window (written into the game's own cfg file, where MAME keeps them). */
#include "common.h"
#include <string>
#include <vector>
#include <map>

static std::string full_tag(const std::string &t) { return !t.empty() && t[0] == ':' ? t : ":" + t; }   /* -listxml writes DSW, the cfg file :DSW */

static std::string attr(const std::string &tag, const char *name) {
    std::string key = std::string(" ") + name + "=\"";
    size_t a = tag.find(key), e;
    if (a == std::string::npos) return "";
    a += key.size();
    e = tag.find('"', a);
    if (e == std::string::npos) return "";
    std::string v = tag.substr(a, e - a);
    for (const char *ent : {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;"}) {
        const char rep = ent[1] == 'a' && ent[2] == 'm' ? '&' : ent[1] == 'l' ? '<' : ent[1] == 'g' ? '>' : ent[1] == 'q' ? '"' : '\'';
        size_t p;
        while ((p = v.find(ent)) != std::string::npos) v.replace(p, strlen(ent), 1, rep);
    }
    return v;
}

static std::map<std::wstring, GameInfo> g_info;

const GameInfo *game_info(const wchar_t *set) {
    auto it = g_info.find(set);
    GameInfo gi;
    Buf out = {0};
    DWORD code = 0;
    std::wstring args = std::wstring(L"-listxml ") + set;
    std::string xml, want;
    size_t pos, end;
    if (it != g_info.end()) return &it->second;
    gi.ok = 0;
    if (mame_run_capture(args.c_str(), &out, 30000, &code) == 0 && out.s) {
        char *u = w_to_u8(set);
        want = std::string("<machine name=\"") + u + "\"";
        free(u);
        xml.assign(out.s, out.n);
        pos = xml.find(want);
        if (pos != std::string::npos) {
            DipSw *cur = NULL;
            end = xml.find("</machine>", pos);
            if (end == std::string::npos) end = xml.size();
            gi.ok = 1;
            while ((pos = xml.find('<', pos + 1)) != std::string::npos && pos < end) {
                size_t e = xml.find('>', pos);
                std::string tag;
                if (e == std::string::npos) break;
                tag = xml.substr(pos + 1, e - pos - 1);
                if (tag.compare(0, 7, "biosset") == 0) {
                    BiosEntry b;
                    b.name = attr(tag, "name"); b.desc = attr(tag, "description"); b.def = attr(tag, "default") == "yes";
                    gi.bios.push_back(b);
                } else if (tag.compare(0, 9, "dipswitch") == 0) {
                    DipSw d;
                    d.name = attr(tag, "name"); d.tag = attr(tag, "tag"); d.mask = strtoul(attr(tag, "mask").c_str(), NULL, 10); d.defv = 0;
                    gi.dips.push_back(d);
                    cur = &gi.dips.back();
                } else if (tag.compare(0, 10, "/dipswitch") == 0) cur = NULL;
                else if (tag.compare(0, 8, "dipvalue") == 0 && cur) {
                    unsigned long v = strtoul(attr(tag, "value").c_str(), NULL, 10);
                    bool dup = false;
                    for (const auto &pv : cur->vals) if (pv.second == v) dup = true;   /* MAME may list a value twice */
                    if (!dup) cur->vals.push_back(std::make_pair(attr(tag, "name"), v));
                    if (attr(tag, "default") == "yes") cur->defv = v;
                }
            }
        }
    }
    buf_free(&out);
    g_info[set] = gi;
    return &g_info[set];
}

/* ---------------------------------------------------------------- the game's cfg file */
static void game_cfg_path(const wchar_t *set, wchar_t *out, size_t cap) {
    wchar_t md[560], dir[600], f[200];
    mame_dir(md, 560);
    path_join(dir, 600, md, L"cfg");
    _snwprintf(f, 200, L"%ls.cfg", set);
    path_join(out, cap, dir, f);
}

typedef struct { std::string tag; unsigned long mask, defv, value; } DipEntry;

/* the value of a DIP switch from the cfg file of the game (the default when there is no entry) */
static unsigned long saved_value(const std::string &cfg, const DipSw &d) {
    size_t pos = 0;
    while ((pos = cfg.find("<port ", pos)) != std::string::npos) {
        size_t e = cfg.find('>', pos);
        std::string head;
        if (e == std::string::npos) break;
        head = cfg.substr(pos, e - pos);
        pos = e;
        if (attr(head, "type") == "DIPSWITCH" && full_tag(attr(head, "tag")) == full_tag(d.tag) && strtoul(attr(head, "mask").c_str(), NULL, 10) == d.mask) {
            std::string v = attr(head, "value");
            return v.empty() ? d.defv : strtoul(v.c_str(), NULL, 10);
        }
    }
    return d.defv;
}

/* the entries of these switches go out of the file text; the changed ones (value differs from the default) come in */
static std::string with_dips(std::string t, const wchar_t *set, const std::vector<DipEntry> &ents, const std::vector<DipSw> &all) {
    size_t pos = 0;
    std::string add, name;
    char *u = w_to_u8(set);
    name = u ? u : "";
    free(u);
    while ((pos = t.find("<port ", pos)) != std::string::npos) {
        size_t e = t.find('>', pos), end;
        std::string head;
        if (e == std::string::npos) break;
        head = t.substr(pos, e - pos);
        if (attr(head, "type") == "DIPSWITCH") {
            bool mine = false;
            for (const DipSw &d : all) if (full_tag(attr(head, "tag")) == full_tag(d.tag) && strtoul(attr(head, "mask").c_str(), NULL, 10) == d.mask) mine = true;
            if (mine) {
                end = e > 0 && t[e - 1] == '/' ? e + 1 : t.find("</port>", pos);
                if (end != std::string::npos && !(t[e - 1] == '/')) end += 7;
                if (end == std::string::npos) break;
                {
                    size_t from = pos, to = end;
                    while (from > 0 && (t[from - 1] == ' ' || t[from - 1] == '\t')) from--;
                    if (to < t.size() && t[to] == '\r') to++;
                    if (to < t.size() && t[to] == '\n') to++;
                    t.erase(from, to - from);
                    pos = from;
                }
                continue;
            }
        }
        pos = e;
    }
    for (const DipEntry &d : ents) {
        char b[300];
        if (d.value == d.defv) continue;
        snprintf(b, sizeof b, "            <port tag=\"%s\" type=\"DIPSWITCH\" mask=\"%lu\" defvalue=\"%lu\" value=\"%lu\" />\n", full_tag(d.tag).c_str(), d.mask, d.defv, d.value);
        add += b;
    }
    if (add.empty()) return t;
    if (t.find("<mameconfig") == std::string::npos) return "<?xml version=\"1.0\"?>\n<mameconfig version=\"10\">\n    <system name=\"" + name + "\">\n        <input>\n" + add + "        </input>\n    </system>\n</mameconfig>\n";
    {
        size_t sys = t.find("<system name=\"" + name + "\">"), in, inEnd, at;
        if (sys == std::string::npos) {
            at = t.rfind("</mameconfig>");
            if (at == std::string::npos) return t;
            t.insert(at, "    <system name=\"" + name + "\">\n        <input>\n" + add + "        </input>\n    </system>\n");
            return t;
        }
        in = t.find("<input>", sys);
        inEnd = t.find("</system>", sys);
        if (in != std::string::npos && inEnd != std::string::npos && in < inEnd) {
            at = t.find("</input>", in);
            if (at == std::string::npos) return t;
            while (at > 0 && (t[at - 1] == ' ' || t[at - 1] == '\t')) at--;
            t.insert(at, add);
        } else if (inEnd != std::string::npos) t.insert(inEnd, "        <input>\n" + add + "        </input>\n    ");
    }
    return t;
}

/* ---------------------------------------------------------------- the DIP Switches window */
typedef struct { const GameInfo *gi; std::vector<HWND> cb; } DipCtx;

static int dip_ok(Modal *md) {
    DipCtx *c = (DipCtx *)md->user;
    return c != NULL;
}

void dlg_dip_switches(HWND owner, const Game *g) {
    const GameInfo *gi = game_info(g->set);
    Modal md;
    DipCtx c;
    wchar_t p[700];
    std::string cfg;
    char *txt;
    size_t i, k;
    int ch, cw = 520;
    if (!gi->ok || gi->dips.empty()) { msg_box(owner, L"DIP Switches", L"This game has no DIP switches, or MAME did not list them.", MB_ICONINFORMATION, NULL); return; }
    game_cfg_path(g->set, p, 700);
    txt = read_file(p, NULL);
    if (txt) { cfg = txt; free(txt); }
    memset(&md, 0, sizeof md);
    c.gi = gi;
    md.kind = 6; md.owner = owner; md.user = &c; md.onok = dip_ok;
    wcscpy(md.title, L"DIP Switches");
    ch = 64 + (int)gi->dips.size() * 30 + 50;
    {
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        int avail = MulDiv(wa.bottom - wa.top, 96, g_dpi) - 90;
        if (ch > avail) ch = avail;
    }
    make_modal(&md, cw, ch);
    {
        wchar_t head[300];
        swprintf(head, 300, L"%ls\nThe DIP switches of the game as MAME keeps them (the game's own cfg file). The game must not be running.", g->title);
        mkat(md.dlg, L"STATIC", head, SS_LEFT | SS_NOPREFIX, 12, 8, cw - 24, 48, 0);
    }
    for (i = 0; i < gi->dips.size() && 64 + (int)i * 30 + 50 <= ch; i++) {
        const DipSw &d = gi->dips[i];
        unsigned long cur = saved_value(cfg, d);
        int sel = 0;
        HWND cb;
        std::wstring nm;
        {
            wchar_t *w = u8_to_w(d.name.c_str(), -1);
            nm = w;
            free(w);
        }
        mkat(md.dlg, L"STATIC", nm.c_str(), SS_LEFT | SS_NOPREFIX, 12, 64 + (int)i * 30 + 4, 220, 18, 0);
        cb = mkat(md.dlg, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 240, 64 + (int)i * 30, cw - 252, 240, 0);
        for (k = 0; k < d.vals.size(); k++) {
            wchar_t *w = u8_to_w(d.vals[k].first.c_str(), -1);
            std::wstring item = w;
            free(w);
            if (d.vals[k].second == d.defv) item += L" (default)";
            SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)item.c_str());
            if (d.vals[k].second == cur) sel = (int)k;
        }
        SendMessageW(cb, CB_SETCURSEL, sel, 0);
        c.cb.push_back(cb);
    }
    mkat(md.dlg, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, cw - 190, ch - 36, 80, 26, IDOK);
    mkat(md.dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, cw - 100, ch - 36, 90, 26, IDCANCEL);
    run_modal(&md);
    if (!md.changed) return;
    {
        std::vector<DipEntry> ents;
        for (i = 0; i < c.cb.size(); i++) {
            DipEntry e;
            int sel = (int)SendMessageW(c.cb[i], CB_GETCURSEL, 0, 0);
            const DipSw &d = gi->dips[i];
            if (sel < 0 || sel >= (int)d.vals.size()) continue;
            e.tag = d.tag; e.mask = d.mask; e.defv = d.defv; e.value = d.vals[sel].second;
            ents.push_back(e);
        }
        {
            std::vector<DipSw> shown(gi->dips.begin(), gi->dips.begin() + c.cb.size());   /* the switches that fit the window: the others keep their saved value */
            std::string out = with_dips(cfg, g->set, ents, shown);
            wchar_t dir[700];
            wcscpy(dir, p);
            { wchar_t *s = wcsrchr(dir, L'\\'); if (s) { *s = 0; CreateDirectoryW(dir, NULL); } }
            if (out != cfg) write_file(p, out.data(), out.size());
        }
    }
}
