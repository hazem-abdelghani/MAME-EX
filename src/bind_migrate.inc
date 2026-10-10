/* ---------------------------------------------------------------- repairing older mapping rows */
static std::string trigger_code(int dev, const char *name, const char *fallbackTok) { (void)fallbackTok; return bind_trigger_code(dev, name[0] == 'R'); }

/* Rows made by older builds hold only what the pad capture wrote: no key (the pad button replaced MAME's default key), one source for a direction,
   the codes of the first LT / RT try (the right stick). They are completed once: the default key comes back, a direction gets the D-pad and the stick,
   old trigger codes become the triggers. */
int bind_migrate(void) {
    int i, changed = 0;
    for (i = 0; i < NBIND; i++) {
        Bind *b = &g_b[i];
        std::vector<std::string> codes, out;
        size_t k;
        int dev = 1, hasHat = 0, hasStick = 0, rowChanged = 0;
        if (!b->custom) continue;
        bind_pad_list(b, codes);
        if (!b->key[0] && kRows[i].defKey[0] && !b->extra[0] && !codes.empty()) { strncpy(b->key, kRows[i].defKey, sizeof b->key - 1); changed = rowChanged = 1; }
        for (k = 0; k < codes.size(); k++) {
            std::string c = codes[k];
            int d = atoi(c.c_str());
            size_t u = c.find('_');
            std::string item = u == std::string::npos ? c : c.substr(u + 1);
            if (d > 0) dev = d;
            if (padprobe_is_xinput(dev)) {   /* only an XInput pad had the triggers on those axes by mistake: on a DirectInput pad ZAXIS is the real trigger axis and RZAXIS the right stick */
                if (item == "RZAXIS_NEG_SWITCH") { c = trigger_code(dev, "RT", "SLIDER2"); changed = rowChanged = 1; }
                else if (item == "ZAXIS_NEG_SWITCH") { c = trigger_code(dev, "LT", "SLIDER1"); changed = rowChanged = 1; }
            }
            if (c.find("HAT") != std::string::npos) hasHat = 1;
            if (c.find("AXIS_") != std::string::npos && c.find("_SWITCH") != std::string::npos && (c.find("XAXIS") != std::string::npos || c.find("YAXIS") != std::string::npos)) hasStick = 1;
            out.push_back(c);
        }
        if (kRows[i].dir && !out.empty() && (!hasHat || !hasStick)) {   /* a direction with only the D-pad or only the stick */
            static const char *hat[4] = {"HAT1UP", "HAT1DOWN", "HAT1LEFT", "HAT1RIGHT"}, *stick[4] = {"YAXIS_UP_SWITCH", "YAXIS_DOWN_SWITCH", "XAXIS_LEFT_SWITCH", "XAXIS_RIGHT_SWITCH"}, *hn[4] = {"D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"};
            int d = i % 10;
            char t[96], tok[48];
            if (!hasHat) { snprintf(t, sizeof t, "%d_%s", dev, padprobe_token(dev, hn[d], tok, sizeof tok, NULL) ? tok : hat[d]); out.push_back(t); changed = rowChanged = 1; }
            if (!hasStick) { snprintf(t, sizeof t, "%d_%s", dev, stick[d]); out.push_back(t); changed = rowChanged = 1; }
        }
        if (rowChanged) bind_pad_store(b, out);
    }
    for (i = 0; i < NBIND; i++) {   /* the buttons that still have an earlier default arrangement (A B X Y LB RB, or A B RT X Y RB) get the current one (X Y RB A B RT) */
        Bind *b = &g_b[i];
        int pl = i < 10 ? 1 : 2, k = i % 10 - 3, dev = bind_pad_dev(pl);
        char first[40], second[40];
        static const int num2[7] = {0, 1, 2, 0, 3, 4, 6};   /* the second arrangement: A B RT X Y RB */
        if (!b->custom || kRows[i].dir || i < 4 || i > 19 || (i > 9 && i < 14) || k < 1 || k > 6 || strstr(b->extra, "JOYCODE_")) continue;
        snprintf(first, sizeof first, "%d_BUTTON%d", dev, k);
        if (k == 3) strncpy(second, bind_trigger_code(dev, 1).c_str(), sizeof second - 1); else snprintf(second, sizeof second, "%d_BUTTON%d", dev, num2[k]);
        second[sizeof second - 1] = 0;
        if (!strcmp(b->pad, first) || !strcmp(b->pad, second)) {
            std::vector<std::string> codes;
            default_pad(i, codes);
            if (codes.size() == 1 && strcmp(b->pad, codes[0].c_str())) { bind_pad_store(b, codes); changed = 1; }
        }
    }
    for (i = 0; i < NBIND; i++) {   /* the new default keys: rows that still have the old default key, and the rows that were MAME's own */
        if (!g_b[i].custom) { make_custom(i); changed = 1; }
        else if (kOldKey[i][0] && !strcmp(g_b[i].key, kOldKey[i]) && strcmp(kOldKey[i], kRows[i].defKey)) { strncpy(g_b[i].key, kRows[i].defKey, sizeof g_b[i].key - 1); changed = 1; }
    }
    if (changed) g_dirty = 1;
    return changed;
}

/* The input settings that MAME saved for a single game (cfg\<set>.cfg, written when the keys are changed in MAME's own Tab menu) win over cfg\default.cfg.
   The ports of the actions of the list are removed from that file, so the mapping of this launcher applies; the first copy is kept as <set>.cfg.mame-ex-backup. */
void bind_strip_game_cfg(const wchar_t *set) {
    wchar_t md[560], dir[600], p[700], bak[720];
    char *txt;
    std::string t, out;
    size_t pos = 0;
    int removed = 0;
    if (!g_set.ovGameCfg || !set || !set[0]) return;
    mame_dir(md, 560);
    path_join(dir, 600, md, L"cfg");
    _snwprintf(p, 700, L"%ls\\%ls.cfg", dir, set);
    txt = read_file(p, NULL);
    if (!txt) return;
    t = txt;
    free(txt);
    while (pos < t.size()) {
        size_t a = t.find("<port ", pos), e;
        if (a == std::string::npos) break;
        e = t.find("</port>", a);
        size_t tagEnd = t.find('>', a);
        if (e == std::string::npos || tagEnd == std::string::npos) break;
        if (tagEnd > 0 && t[tagEnd - 1] == '/') { pos = tagEnd + 1; continue; }   /* an empty <port .../> */
        {
            std::string head = t.substr(a, tagEnd - a), blk = t.substr(a, e + 7 - a);
            size_t ty = head.find("type=\"");
            bool ours = false;
            if (ty != std::string::npos) {
                size_t q = head.find('"', ty + 6);
                std::string name = head.substr(ty + 6, q == std::string::npos ? std::string::npos : q - ty - 6);
                for (int i = 0; i < NBIND; i++) if (name == kRows[i].token) ours = true;
            }
            if (ours && blk.find("<newseq") != std::string::npos) {
                size_t from = a, to = e + 7;
                while (from > 0 && (t[from - 1] == ' ' || t[from - 1] == '\t')) from--;
                if (to < t.size() && t[to] == '\r') to++;
                if (to < t.size() && t[to] == '\n') to++;
                t.erase(from, to - from);
                pos = from;
                removed++;
                continue;
            }
        }
        pos = e + 7;
    }
    if (!removed) return;
    _snwprintf(bak, 720, L"%ls.mame-ex-backup", p);
    if (!file_exists(bak)) { char *orig = read_file(p, NULL); if (orig) { write_file(bak, orig, strlen(orig)); free(orig); } }
    write_file(p, t.data(), t.size());
}
