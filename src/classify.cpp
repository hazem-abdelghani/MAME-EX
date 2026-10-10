/* Guesses the maker, the genre and the region of a game from MAME's manufacturer text and title, for the list filters.
   No Windows code: testable on any host. */
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include "classify.h"

static int has(const wchar_t *lowTitle, const wchar_t *word) { return wcsstr(lowTitle, word) != NULL; }

static void lower(const wchar_t *s, wchar_t *out, int cap) {
    int i;
    for (i = 0; s[i] && i < cap - 1; i++) out[i] = (wchar_t)towlower((wint_t)s[i]);
    out[i] = 0;
}

typedef struct { const wchar_t *word, *label; } Rule;

static const wchar_t *first_match(const wchar_t *low, const Rule *r) {
    for (; r->word; r++) if (has(low, r->word)) return r->label;
    return NULL;
}

/* "Capcom / Arika" -> Capcom, "Axela / Namco" -> Namco, "Eighting / Raizing / Namco" -> Namco, "Tecmo" -> Tecmo, "Warashi / Mahjong Kobo / Taito" -> Taito ... */
const wchar_t *classify_maker(const wchar_t *maker) {
    static const Rule byName[] = {
        {L"snk", L"SNK"}, {L"namco", L"Namco"}, {L"capcom", L"Capcom"}, {L"konami", L"Konami"}, {L"taito", L"Taito"}, {L"tecmo", L"Tecmo"},
        {L"atlus", L"Atlus"}, {L"acclaim", L"Acclaim"}, {L"atari", L"Atari"}, {L"video system", L"Video System"}, {L"mco'river", L"Video System"},
        {L"success", L"Success"}, {L"warashi", L"Warashi"}, {L"takumi", L"Takumi"}, {L"cyberfront", L"CyberFront"}, {L"sunsoft", L"Sunsoft"}, {L"arika", L"Arika"},
        {L"square", L"Square"}, {L"eighting", L"Eighting / Raizing"}, {L"raizing", L"Eighting / Raizing"}, {L"moss", L"MOSS"},
        {L"metro", L"Metro"}, {L"gaps", L"Gaps"}, {L"virtual music", L"Virtual Music"}, {L"produce", L"Produce"}, {NULL, NULL}};
    wchar_t m[120];
    const wchar_t *r;
    lower(maker ? maker : L"", m, 120);
    if ((r = first_match(m, byName))) return r;
    return L"Other";
}

const wchar_t *classify_genre(const wchar_t *title) {
    static const Rule rules[] = {
        {L"tekken", L"Fighting"}, {L"street fighter", L"Fighting"}, {L"rival schools", L"Fighting"}, {L"star gladiator", L"Fighting"},
        {L"plasma sword", L"Fighting"}, {L"soul edge", L"Fighting"}, {L"soul calibur", L"Fighting"}, {L"toshinden", L"Fighting"},
        {L"ehrgeiz", L"Fighting"}, {L"fighting layer", L"Fighting"}, {L"toukon", L"Fighting"}, {L"tech romancer", L"Fighting"},
        {L"kikaioh", L"Fighting"}, {L"justice gakuen", L"Fighting"}, {L"fighting", L"Fighting"},
        {L"samurai", L"Fighting"}, {L"fatal fury", L"Fighting"}, {L"buriki", L"Fighting"},
        {L"dance dance revolution", L"Music"}, {L"dancing stage", L"Music"}, {L"stepping", L"Music"}, {L"beatmania", L"Music"}, {L"guitar", L"Music"},
        {L"drummania", L"Music"}, {L"keyboardmania", L"Music"}, {L"pop'n", L"Music"}, {L"para para", L"Music"}, {L"mambo", L"Music"}, {L"dj ", L"Music"},
        {L"dancemania", L"Music"}, {L"ddr", L"Music"}, {L"jam", L"Music"}, {L"percussion", L"Music"}, {L"gunmania", L"Shooter"}, {L"silent scope", L"Shooter"},
        {L"raycrisis", L"Shooter"}, {L"raystorm", L"Shooter"}, {L"ray storm", L"Shooter"}, {L"darius", L"Shooter"}, {L"shikigami", L"Shooter"}, {L"xii stag", L"Shooter"},
        {L"psyvariar", L"Shooter"}, {L"aero fighters", L"Shooter"}, {L"sonic wings", L"Shooter"}, {L"chaos heat", L"Shooter"}, {L"brave blade", L"Shooter"},
        {L"psychic force", L"Fighting"}, {L"fighters' impact", L"Fighting"}, {L"bloody roar", L"Fighting"}, {L"beastorizer", L"Fighting"},
        {L"mahjong", L"Mahjong"}, {L"majan", L"Mahjong"}, {L"shanghai", L"Puzzle"}, {L"puzzle bobble", L"Puzzle"}, {L"bust a move", L"Puzzle"}, {L"flip maze", L"Puzzle"},
        {L"steering champ", L"Racing"}, {L"handle champ", L"Racing"}, {L"rc de go", L"Racing"}, {L"go by rc", L"Racing"}, {L"gallop", L"Racing"}, {L"derby", L"Racing"},
        {L"bowling", L"Sports"}, {L"yakyuu", L"Sports"}, {L"hyper athlete", L"Sports"}, {L"golf", L"Sports"}, {L"hockey", L"Sports"}, {L"football", L"Sports"},
        {L"strider", L"Action"}, {L"beast busters", L"Shooter"}, {L"golgo", L"Shooter"},
        {L"point blank", L"Shooter"}, {L"gunbarl", L"Shooter"}, {L"ghoul panic", L"Shooter"}, {L"bakyuuun", L"Shooter"}, {L"xevious", L"Shooter"},
        {L"star sweep", L"Shooter"}, {L"tenkomori", L"Shooter"}, {L"shooting", L"Shooter"}, {L"flame gunner", L"Shooter"},
        {L"racer", L"Racing"}, {L"road", L"Racing"}, {L"rally", L"Racing"}, {L"racing", L"Racing"}, {L"drive", L"Racing"}, {L"kart", L"Racing"}, {L"truck", L"Racing"}, {L"rail", L"Racing"},
        {L"libero grande", L"Sports"}, {L"dunk", L"Sports"}, {L"prime goal", L"Sports"}, {L"stadium", L"Sports"}, {L"family bowl", L"Sports"},
        {L"soccer", L"Sports"}, {L"football", L"Sports"}, {L"baseball", L"Sports"}, {L"world cup", L"Sports"},
        {L"driller", L"Puzzle"}, {L"tetris", L"Puzzle"}, {L"block kuzushi", L"Puzzle"}, {L"logic pro", L"Puzzle"}, {L"puzzle", L"Puzzle"},
        {L"quiz", L"Quiz"}, {L"paca paca", L"Music"}, {L"jammer lammy", L"Music"}, {L"dancing eyes", L"Music"}, {L"quest for fame", L"Music"},
        {NULL, NULL}};
    wchar_t t[300];
    const wchar_t *g;
    lower(title, t, 300);
    if ((g = first_match(t, rules))) return g;
    return L"Other";
}

/* the region from the words between ( ) in the title, e.g. "Tekken 3 (US, TET3/VER.A)": US, JP, ASIA, WORLD, EUROPE, or Other when none says it.
   Whole words only ("us" is not found inside "plus" or "austria"); the first region word of the first group that has one wins. */
const wchar_t *classify_region(const wchar_t *title) {
    static const Rule words[] = {
        {L"us", L"US"}, {L"usa", L"US"}, {L"jp", L"JP"}, {L"jpn", L"JP"}, {L"japan", L"JP"}, {L"japanese", L"JP"},
        {L"asia", L"ASIA"}, {L"world", L"WORLD"}, {L"europe", L"EUROPE"}, {NULL, NULL}};
    const wchar_t *p = title ? title : L"";
    while ((p = wcschr(p, L'(')) != NULL) {
        wchar_t w[16];
        int n = 0;
        for (p++; *p && *p != L')'; p++) {
            if (iswalnum((wint_t)*p)) { if (n < 15) w[n++] = (wchar_t)towlower((wint_t)*p); }
            else if (n) { const Rule *r; w[n] = 0; for (r = words; r->word; r++) if (!wcscmp(w, r->word)) return r->label; n = 0; }
        }
        if (n) { const Rule *r; w[n] = 0; for (r = words; r->word; r++) if (!wcscmp(w, r->word)) return r->label; }
        if (!*p) break;
    }
    return L"Other";
}
