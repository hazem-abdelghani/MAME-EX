/* "All MAME Options": every command line option of MAME that the other tabs do not have controls for (core, OSD and Windows options with their description;
   the list comes from the MAME sources: tools/gen_options.py). Only values that differ from MAME's defaults are passed, as command line words kept in
   Settings.moreOpts ("-bios us -nonvram_save -gamma 1.2"). Options that do not exist in an older MAME make it stop with an error: change only what you need. */
#include "common.h"
#include <string>
#include <vector>

typedef struct { const char *group, *name; char kind; const char *def, *lo, *hi, *desc; } MOpt;
static const MOpt kM[] = {
#include "mameopts_data.inc"
};
#define NM ((int)(sizeof kM / sizeof kM[0]))

typedef struct {
    std::string val[NM];
    std::vector<int> shown;   /* the rows of the list: indexes into kM (the search filters them) */
    HWND lv, search, info;
} MCtx;

static std::wstring wa(const char *s) { wchar_t *w = u8_to_w(s, -1); std::wstring r = w; free(w); return r; }
static std::string ua(const wchar_t *s) { char *u = w_to_u8(s); std::string r = u ? u : ""; free(u); return r; }

static void parse_words(const wchar_t *text, MCtx *c) {
    int n = 0, i, k;
    wchar_t **w;
    for (i = 0; i < NM; i++) c->val[i] = kM[i].def;
    if (!text[0]) return;
    w = CommandLineToArgvW((std::wstring(L"x ") + text).c_str(), &n);
    if (!w) return;
    for (k = 1; k < n; k++) {
        std::string tok = ua(w[k]);
        if (tok.size() < 2 || tok[0] != '-') continue;
        std::string name = tok.substr(1);
        for (i = 0; i < NM; i++) {
            if (name == kM[i].name) {
                if (kM[i].kind == 'B') c->val[i] = "1";
                else if (k + 1 < n) c->val[i] = ua(w[++k]);
                break;
            }
            if (kM[i].kind == 'B' && name == std::string("no") + kM[i].name) { c->val[i] = "0"; break; }
        }
    }
    LocalFree(w);
}

/* one word for the command line, in the form CommandLineToArgvW reads back (blanks, quotes and a trailing backslash of a folder) */
static std::wstring quote_word(const std::wstring &v) {
    std::wstring o;
    size_t i, bs;
    if (!v.empty() && v.find_first_of(L" \t\"") == std::wstring::npos) return v;
    o = L"\"";
    for (i = 0, bs = 0; i < v.size(); i++) {
        if (v[i] == L'\\') { bs++; o += L'\\'; continue; }
        if (v[i] == L'"') o.append(bs + 1, L'\\');
        bs = 0;
        o += v[i];
    }
    o.append(bs, L'\\');   /* a trailing backslash must not swallow the closing quote */
    return o + L"\"";
}

static std::wstring words_of(const MCtx *c) {   /* only what differs from MAME's defaults */
    std::wstring out;
    for (int i = 0; i < NM; i++) {
        if (c->val[i] == kM[i].def) continue;
        if (!out.empty()) out += L" ";
        if (kM[i].kind == 'B') out += std::wstring(c->val[i] == "1" ? L"-" : L"-no") + wa(kM[i].name);
        else {
            std::wstring v = wa(c->val[i].c_str());
            if (v.empty()) { out.resize(out.size() > 0 ? out.size() - 1 : 0); continue; }
            out += L"-" + wa(kM[i].name) + L" " + quote_word(v);
        }
    }
    return out;
}

static std::wstring show_val(const MCtx *c, int i) {
    if (kM[i].kind == 'B') return c->val[i] == "1" ? L"On" : L"Off";
    return c->val[i].empty() ? std::wstring(L"(none)") : wa(c->val[i].c_str());
}
static std::wstring show_def(int i) {
    if (kM[i].kind == 'B') return std::string(kM[i].def) == "1" ? L"On" : L"Off";
    return kM[i].def[0] ? wa(kM[i].def) : std::wstring(L"(none)");
}

static void refill(MCtx *c) {
    wchar_t q[100];
    std::wstring f;
    int i;
    GetWindowTextW(c->search, q, 100);
    f = q;
    for (auto &ch : f) ch = (wchar_t)towlower(ch);
    c->shown.clear();
    ListView_DeleteAllItems(c->lv);
    for (i = 0; i < NM; i++) {
        if (!f.empty()) {
            std::wstring hay = wa(kM[i].name) + L" " + wa(kM[i].desc) + L" " + wa(kM[i].group);
            for (auto &ch : hay) ch = (wchar_t)towlower(ch);
            if (hay.find(f) == std::wstring::npos) continue;
        }
        {
            LVITEMW it;
            std::wstring g = wa(kM[i].group), nm = wa(kM[i].name), v = show_val(c, i), d = show_def(i);
            int row = (int)c->shown.size();
            memset(&it, 0, sizeof it);
            it.mask = LVIF_TEXT; it.iItem = row; it.pszText = (LPWSTR)g.c_str();
            ListView_InsertItem(c->lv, &it);
            ListView_SetItemText(c->lv, row, 1, (LPWSTR)nm.c_str());
            ListView_SetItemText(c->lv, row, 2, (LPWSTR)v.c_str());
            ListView_SetItemText(c->lv, row, 3, (LPWSTR)d.c_str());
            c->shown.push_back(i);
        }
    }
}

static int cur_opt(MCtx *c) {
    int sel = ListView_GetNextItem(c->lv, -1, LVNI_SELECTED);
    return sel >= 0 && sel < (int)c->shown.size() ? c->shown[sel] : -1;
}

static void show_row(MCtx *c, int opt) {
    for (int r = 0; r < (int)c->shown.size(); r++)
        if (c->shown[r] == opt) { std::wstring v = show_val(c, opt); ListView_SetItemText(c->lv, r, 2, (LPWSTR)v.c_str()); }
}

static int valid(const MOpt &o, const std::string &v) {
    char *e = NULL;
    if (o.kind == 'I' || o.kind == 'F') {
        double x;
        if (v.empty()) return 0;
        x = strtod(v.c_str(), &e);
        if (!e || *e) return 0;
        if (o.kind == 'I' && x != (long long)x) return 0;
        if (o.lo[0] && x < atof(o.lo)) return 0;
        if (o.hi[0] && x > atof(o.hi)) return 0;
    }
    return 1;
}

static void edit_opt(Modal *md, int i) {
    MCtx *c = (MCtx *)md->user;
    if (i < 0) return;
    if (kM[i].kind == 'B') c->val[i] = c->val[i] == "1" ? "0" : "1";
    else {
        wchar_t t[400], prompt[300];
        std::wstring cur = wa(c->val[i].c_str()), nm = wa(kM[i].name);
        wcsncpy(t, cur.c_str(), 399); t[399] = 0;
        if (kM[i].lo[0] || kM[i].hi[0]) swprintf(prompt, 300, L"-%ls (%hs to %hs; empty = MAME's default):", nm.c_str(), kM[i].lo[0] ? kM[i].lo : "...", kM[i].hi[0] ? kM[i].hi : "...");
        else swprintf(prompt, 300, L"-%ls (empty = MAME's default):", nm.c_str());
        if (!dlg_text(md->dlg, L"MAME Option", prompt, t, 400)) return;
        {
            std::string v = ua(t);
            size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
            v = a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
            if (v.empty()) v = kM[i].def;
            else if (!valid(kM[i], v)) { msg_box(md->dlg, L"MAME Option", L"This value is not valid for the option.", MB_ICONWARNING, NULL); return; }
            c->val[i] = v;
        }
    }
    show_row(c, i);
}

static int more_cmd(Modal *md, int id) {
    MCtx *c = (MCtx *)md->user;
    int i = cur_opt(c);
    if (id == 1001) { edit_opt(md, i); return 1; }
    if (id == 1002) { if (i >= 0) { c->val[i] = kM[i].def; show_row(c, i); } return 1; }
    if (id == 1003) {
        int r = 0, k;
        msg_box(md->dlg, L"MAME Options", L"Give all the options of this window their MAME defaults?", MB_YESNO | MB_ICONQUESTION, &r);
        if (r == IDYES) { for (k = 0; k < NM; k++) c->val[k] = kM[k].def; refill(c); }
        return 1;
    }
    if (id == 1004) { refill(c); return 1; }   /* the search box changed */
    return 0;
}

static LRESULT more_notify(Modal *md, NMHDR *nh) {
    MCtx *c = (MCtx *)md->user;
    if (nh->hwndFrom != c->lv) return 0;
    if (nh->code == NM_DBLCLK) { edit_opt(md, cur_opt(c)); return 0; }
    if (nh->code == LVN_ITEMCHANGED) {
        NMLISTVIEW *lv = (NMLISTVIEW *)nh;
        if ((lv->uChanged & LVIF_STATE) && (lv->uNewState & LVIS_SELECTED)) {
            int i = cur_opt(c);
            if (i >= 0) {
                std::wstring t = L"-" + wa(kM[i].name) + L":  " + wa(kM[i].desc);
                SetWindowTextW(c->info, t.c_str());
            }
        }
        return 0;
    }
    if (nh->code == NM_CUSTOMDRAW) return lv_altrows((LPNMLVCUSTOMDRAW)nh);
    return 0;
}

static int more_ok(Modal *md) {
    MCtx *c = (MCtx *)md->user;
    std::wstring w = words_of(c);
    if (w.size() >= sizeof g_set.moreOpts / sizeof(wchar_t)) { msg_box(md->dlg, L"MAME Options", L"Too many changed options for the settings file. Reset some of them.", MB_ICONWARNING, NULL); return 0; }
    wcsncpy(g_set.moreOpts, w.c_str(), sizeof g_set.moreOpts / sizeof(wchar_t) - 1);
    g_set.moreOpts[sizeof g_set.moreOpts / sizeof(wchar_t) - 1] = 0;
    return 1;
}

/* a text box that tells its parent when its text changes (WM_COMMAND EN_CHANGE with the id 1004): the modal window passes ids from 1000 to oncmd */
void more_options_dialog(HWND owner) {
    Modal md;
    MCtx *c = new MCtx();
    LVCOLUMNW col;
    RECT wa_;
    int cw = 720, ch = 600, avail;
    memset(&md, 0, sizeof md);
    md.kind = 9; md.owner = owner; md.user = c; md.onok = more_ok; md.oncmd = more_cmd; md.onnotify = more_notify;
    wcscpy(md.title, L"All MAME Options");
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa_, 0);
    avail = MulDiv(wa_.bottom - wa_.top, 96, g_dpi) - 90;   /* not taller than the screen */
    if (ch > avail) ch = avail > 400 ? avail : 400;
    make_modal(&md, cw, ch);
    parse_words(g_set.moreOpts, c);
    mkat(md.dlg, L"STATIC", L"Search:", SS_LEFT, 10, 14, 56, 18, 0);
    c->search = mkat(md.dlg, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 70, 10, 260, 24, 1004);
    SetWindowLongPtrW(c->search, GWL_EXSTYLE, GetWindowLongPtrW(c->search, GWL_EXSTYLE) | WS_EX_CLIENTEDGE);
    mkat(md.dlg, L"BUTTON", L"Reset All", BS_PUSHBUTTON | WS_TABSTOP, cw - 120, 9, 110, 26, 1003);
    c->lv = mkat(md.dlg, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS | WS_TABSTOP | WS_VSCROLL | WS_BORDER, 10, 44, cw - 20, ch - 44 - 120, 5000);
    ListView_SetExtendedListViewStyle(c->lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    memset(&col, 0, sizeof col);
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"Group"; col.cx = S(150); ListView_InsertColumn(c->lv, 0, &col);
    col.pszText = (LPWSTR)L"Option"; col.cx = S(190); ListView_InsertColumn(c->lv, 1, &col);
    col.pszText = (LPWSTR)L"Value"; col.cx = S(150); ListView_InsertColumn(c->lv, 2, &col);
    col.pszText = (LPWSTR)L"Default"; col.cx = S(150); ListView_InsertColumn(c->lv, 3, &col);
    c->info = mkat(md.dlg, L"STATIC", L"Double-click an option to change its value. Only values that differ from MAME's defaults are passed to MAME.", SS_LEFT, 10, ch - 70, cw - 20, 36, 0);
    mkat(md.dlg, L"BUTTON", L"Edit Value…", BS_PUSHBUTTON | WS_TABSTOP, 10, ch - 34, 110, 26, 1001);
    mkat(md.dlg, L"BUTTON", L"Reset Value", BS_PUSHBUTTON | WS_TABSTOP, 130, ch - 34, 110, 26, 1002);
    mkat(md.dlg, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, cw - 190, ch - 34, 80, 26, IDOK);
    mkat(md.dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, cw - 100, ch - 34, 90, 26, IDCANCEL);
    refill(c);
    run_modal(&md);
    delete c;
}

/* The values of single options for the controls of the other tabs (Display, Capture And Speed, Light Gun ...): they live in the same words as the
   All MAME Options window, so the two always agree. */
static int find_opt(const char *name) {
    for (int i = 0; i < NM; i++) if (!strcmp(kM[i].name, name)) return i;
    return -1;
}

std::wstring mo_get(const char *name) {
    MCtx c;
    int i = find_opt(name);
    if (i < 0) return L"";
    parse_words(g_set.moreOpts, &c);
    if (kM[i].kind == 'B') return c.val[i] == "1" ? L"1" : L"0";
    return wa(c.val[i].c_str());
}

int mo_valid(const char *name, const wchar_t *value) {   /* 1 when the text is acceptable for the option ("" is: the default) */
    int i = find_opt(name);
    std::string v = ua(value);
    if (i < 0) return 0;
    if (v.empty() || kM[i].kind == 'B') return 1;
    return valid(kM[i], v);
}

void mo_set(const char *name, const wchar_t *value) {   /* "" = MAME's default */
    MCtx c;
    int i = find_opt(name);
    std::string v = ua(value);
    if (i < 0) return;
    parse_words(g_set.moreOpts, &c);
    if (kM[i].kind == 'B') v = v == "1" ? "1" : "0";
    else {
        size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
        v = a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
        if (v.empty()) v = kM[i].def;
        else if (!valid(kM[i], v)) return;
    }
    if (c.val[i] == v) return;
    {
        std::string old = c.val[i];
        std::wstring w;
        c.val[i] = v;
        w = words_of(&c);
        if (w.size() >= sizeof g_set.moreOpts / sizeof(wchar_t)) { c.val[i] = old; return; }   /* too long: keep the old value rather than cut a word */
        wcscpy(g_set.moreOpts, w.c_str());
    }
}
