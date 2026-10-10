/* MAME EX: native Windows launcher (C++ / Win32) for MAME, Sony ZN-1 / ZN-2 and Namco System 11 / 12 only. Shared declarations. */
#ifndef ZGUI_COMMON_H
#define ZGUI_COMMON_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0600
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <utility>

/* ---------------------------------------------------------------- util.c */
typedef struct { int *v; int n, cap; } IntList;
void il_clear(IntList *l);
void il_add(IntList *l, int x);
int il_has(const IntList *l, int x);
void il_remove(IntList *l, int x);
void il_copy(IntList *dst, const IntList *src);

typedef struct { char *s; size_t n, cap; } Buf;   /* growable byte string (UTF-8 / ANSI text) */
void buf_add(Buf *b, const char *s);
void buf_addn(Buf *b, const char *s, size_t n);
void buf_fmt(Buf *b, const char *fmt, ...);
void buf_free(Buf *b);

wchar_t *wdup(const wchar_t *s);
wchar_t *u8_to_w(const char *s, int len);        /* malloc'ed */
char *w_to_u8(const wchar_t *s);                 /* malloc'ed */
char *read_file(const wchar_t *path, size_t *len);   /* malloc'ed, NUL terminated, NULL if missing */
int write_file(const wchar_t *path, const void *data, size_t len);
int file_exists(const wchar_t *path);
void plugin_path(wchar_t *out, size_t cap, const wchar_t *name);   /* <launcher folder>\\mame-ex-plugins\\name (the folder is created) */
void log_path(wchar_t *out, size_t cap, const wchar_t *name);   /* <launcher folder>\\logs\\name (the folder is created) */
void path_join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *b);
int is_abs_path(const wchar_t *p);
void trim_w(wchar_t *s);
char *trim_a(char *s);                            /* in place, returns start */
int xatoi(const char *s, int *out);               /* 1 when s holds an integer */

/* ---------------------------------------------------------------- tips.c */
const wchar_t *tip_for(const wchar_t *label);

/* ---------------------------------------------------------------- gamecfg.cpp: settings of single games */
/* every field: -1 = same as the main settings (kSame), else the value of that setting */
#define kSame (-1)
typedef struct {
    int id;
    int video;                   /* 0 auto, 1 bgfx, 2 d3d, 3 opengl, 4 gdi */
    int fullscreen;              /* 0 window, 1 fullscreen */
    int resW, resH;              /* 0 = same as global */
    int rotate;                  /* 0 game default, 1 none, 2 right, 3 left, 4 180 */
    int keepAspect, filter, waitVsync;   /* 0 / 1 */
    int chain;                   /* bgfx screen chain, see kChains */
    int sound;                   /* 0 off, 1 on */
    int volume;                  /* attenuation in dB, 0 .. -32; -100 = same */
    int cheat, skipInfo, autosave;   /* 0 / 1 */
    wchar_t bios[40];            /* -bios for this game ("" = the default BIOS) */
    wchar_t extra[260];          /* extra command line words for this game */
} GameCfg;
typedef struct { int fullscreen; int noFilter; } PlayOpts;   /* one-off changes of a start; fullscreen -1 = unchanged */
void gamecfg_load(void);
int gamecfg_save(void);
const GameCfg *gamecfg_find(int id);            /* NULL when the game has no settings of its own */
int gamecfg_active(const GameCfg *c);
void gamecfg_set(const GameCfg *c);
void gamecfg_remove(int id);
int ui_mode_count(void);                        /* main.cpp: the screen modes of the Video tab */
void ui_mode_get(int i, int *w, int *h);

/* ---------------------------------------------------------------- settings.cpp */
typedef struct {
    /* the fields up to "extra" are the ones the widgets of the tabs edit: Cancel puts them back (see snap_restore in main.cpp) */
    wchar_t mame[520];          /* mame.exe ("" = look beside the launcher, then in .\mame) */
    wchar_t roms[520];          /* ROMs folder(s), several separated by ; ("" = what MAME's own settings say) */
    wchar_t biosDir[520];       /* BIOS folder ("" = the BIOS sets are in the ROMs folder, as before) */
    int hideConsole;
    int onStart;                /* when a game starts: 0 keep the launcher open, 1 minimize it (it comes back when the game ends), 2 close it */
    int logs;                   /* Enable Logs: mame-ex-launch.log, mame-ex-output.log, mame-ex-combos.log, mame-ex-input.log (off: none is written) */
    /* video */
    int video, fullscreen, resW, resH, rotate, keepAspect, unevenStretch, filter, prescale, waitVsync, syncRefresh, throttle, frameskip, maximize;
    int chain, backend, artCrop, hlsl, glsl, glslFilter, intScale;
    int fxOn, fxScaler, fxFxaa, fxDither;   /* the filter window (filter.cpp): scaler 0 smooth, 1 sharp, 2 xBRZ, 3 sharp bilinear, 4 bicubic; dither 0..3; its shape follows Keep Aspect Ratio (4:3, or the whole screen) */
    wchar_t glslShader[64];     /* the name of a GLSL shader set of the glsl folder of MAME ("" = none) */
    /* audio */
    int sound, sampleRate, volume, latency;
    /* controls */
    int joystick, joyProvider, mouse, lightgun, deadzone, saturation, contradictory, afRate;   /* afRate: autofire speed 0..3 (10, 15, 20, 30 presses per second) */
    int ovGameCfg;              /* the launcher mapping wins over the input settings MAME saved for a single game */
    wchar_t ctrlr[64];
    wchar_t profile[64];        /* the controls profile that was chosen last ("" = none) */
    /* advanced */
    int skipInfo, skipWarn, autosave, cheat, confirmQuit;
    wchar_t extra[520];
    wchar_t moreOpts[2048];     /* the options of the "All MAME Options" window that differ from MAME's defaults, as command line words (moreopts.cpp) */
    wchar_t hlslOpts[1024];     /* the HLSL options that differ from MAME's defaults, as command line words (hlsl.cpp) */
    int onlyAvail, favOnly, bindVer;   /* bindVer: 2 once the old mapping rows were repaired */
    wchar_t fRegion[8], fMaker[40], fGenre[24], fHw[40];   /* the list filters of the last session ("" = all) */
    int sortCol, sortAsc;       /* game list sorting: column 0..6, ascending flag */
    int dark;                   /* dark theme (applied at the next start) */
    int themeMode;              /* 0 follow Windows, 1 light, 2 dark (applied at the next start); dark is what it comes to */
    int hideSearch, hideFilters, hideToolbar;   /* the search bar / the filters bar of the game list are hidden */
    int colsHidden;             /* game list columns that are hidden (bit 0 Favorite, 1 #, 2 Game, 3 Info, 4 Status, 5 Hardware, 6 Year); Info is hidden by default */
    int optWindow;              /* the options (Video, Audio ... tabs) are in a window of their own */
    int colsGame[7], splitX;    /* column widths of the game list and the splitter position (96 dpi pixels, 0 = default) */
    IntList fav;
    IntList recent;
} Settings;

/* the choices of the combo boxes (settings.cpp): index 0 is always "leave it to MAME" where the option has a MAME default */
extern const wchar_t *const kVideoModes[5];
extern const wchar_t *const kChains[6];
extern const wchar_t *const kBackends[6];
extern const wchar_t *const kRotations[5];
extern const wchar_t *const kSoundModes[5];
extern const wchar_t *const kProviders[5];
extern const int kSampleRates[5];
extern const char *const kVideoArgs[5], *const kChainArgs[6], *const kBackendArgs[6], *const kSoundArgs[5], *const kProviderArgs[5];   /* the words MAME knows them by */

typedef struct { int x, y, w, h; } WinRect;

extern Settings g_set;
extern WinRect g_winRect, g_optRect;   /* the main window and the options window of the last session */
extern wchar_t g_root[560], g_settingsFile[560];

void settings_defaults(Settings *s, const Settings *keep);
void settings_load(void);
int settings_save(void);                           /* 0 when the file could not be written */
void settings_sig(Buf *b);                         /* what "Save settings" would write (from the fields) */
void set_favorite(int id, int on);
void settings_resolve_theme(void);   /* g_set.dark from g_set.themeMode (and the Windows setting) */

/* ---------------------------------------------------------------- mame.cpp: finding and starting MAME */
typedef struct { wchar_t **a; int n; } ArgList;
void args_free(ArgList *l);
int mame_exe(wchar_t *out, size_t cap);              /* the full path of mame.exe; 1 when it exists */
void mame_dir(wchar_t *out, size_t cap);             /* the folder of mame.exe (the working directory of MAME) */
void roms_dir_abs(wchar_t *out, size_t cap);         /* the first ROMs folder as a full path (the MAME folder's roms when none is set) */
int mame_ctrlr_names(wchar_t names[][64], int max);  /* the controller files of MAME's ctrlr folder */
void mame_args(int gameId, const PlayOpts *po, ArgList *out);   /* the command line without the program: the set name first */
BOOL mame_start(ArgList *args, PROCESS_INFORMATION *pi);   /* MAME's own messages go to mame-ex-output.log */
wchar_t *mame_output_tail(size_t maxChars);          /* malloc'ed: the last lines MAME printed in its last start ("" when none) */
int mame_run_capture(const wchar_t *args, Buf *out, DWORD timeoutMs, DWORD *exitCode);   /* runs mame.exe hidden with a command line, its output into out; 0 on success */
int run_game_cli(const wchar_t *game, int fullscreen);   /* frontends: --game <set or ROM file>; returns the exit code */
wchar_t *args_join(const ArgList *a);                               /* malloc'ed: arg arg ... (blanks quoted) */
wchar_t *command_line_join(const wchar_t *exe, const ArgList *a);   /* malloc'ed: "exe" arg arg ... */
void mame_log_start(const ArgList *a);                              /* Enable Logs: the command line of the start goes to mame-ex-launch.log */

/* ---------------------------------------------------------------- combos.cpp: the Combos tab (move macros played by a MAME Lua plugin) */
typedef struct { const char *kind, *name, *seq, *cmd; } ComboMove;
typedef struct { const char *name; const ComboMove *moves; int nmoves; } ComboChar;
extern const ComboChar g_comboChars[];
extern const int g_nComboChars;
typedef struct { const char *name; int w, h; const unsigned char *bgra; } IconData;   /* icon_data.cpp, made by tools/gen_icons.py from the PNG files in tools/icons */
extern const IconData g_icons[];
extern const int g_nIcons;
void cmb_create(HWND page, HWND tipWnd);
void cmb_layout(int w, int h);
void cmb_load(void);                                /* the combos of mame-ex-combos.cfg into the tab (changes that were not confirmed are dropped) */
void cmb_save(void);                                /* the tab into mame-ex-combos.cfg */
int cmb_command(int id, int code);
int cmb_notify(NMHDR *nh);
void cmb_cell(int row, int col);
int combos_plugin_prepare(wchar_t *pluginsPath, size_t cap);   /* writes the Lua plugin; 1 when a combo is set up (pluginsPath = the folder for -pluginspath) */
/* padprobe.cpp: what MAME calls the controls of the pads of this PC */
#ifdef __cplusplus
typedef struct { std::string name, desc; int def; } BiosEntry;
typedef struct { std::string name, tag; unsigned long mask, defv; std::vector<std::pair<std::string, unsigned long>> vals; } DipSw;
typedef struct { std::vector<BiosEntry> bios; std::vector<DipSw> dips; int ok; } GameInfo;
const GameInfo *game_info(const wchar_t *set);       /* the BIOS versions and DIP switches of a game from mame -listxml (cached) */
#endif
void padprobe_run(int force);                       /* once per session (force: again) */
int padprobe_devno(int user1);                      /* the JOYCODE number of the n-th Xbox-type pad, 0 when unknown */
int padprobe_token(int devno, const char *name, char *tok, size_t cap, int *cur);
int padprobe_is_xinput(int devno);
int padprobe_itemname(int devno, const char *token, char *name, size_t cap);
void more_options_dialog(HWND owner);
#ifdef __cplusplus
std::wstring mo_get(const char *name);               /* the value of one MAME option from the All MAME Options store (the default when it was not changed) */
void mo_set(const char *name, const wchar_t *value);   /* ... set ("" = MAME's default); a value that is not valid is ignored */
int mo_valid(const char *name, const wchar_t *value);
#endif               /* the All MAME Options window (moreopts.cpp) */
void hlsl_dialog(HWND owner);                     /* the HLSL Settings window (hlsl.cpp) */
int diag_plugin_prepare(wchar_t *dirOut, size_t cap);   /* the input log plugin (mame-ex-input.log) */
void mame_probe_args(ArgList *a, const wchar_t *pluginDir);   /* the command line of the probe run */
int capture_key_token(HWND owner, const wchar_t *label, char *out48, int *cleared);
int capture_pad_token(HWND owner, const wchar_t *label, int dir, char *out72);
void key_display(const char *token, wchar_t *out, size_t cap);
void pad_display(const char *code, wchar_t *out, size_t cap);
#define CMB_NTOK 30
#define WM_APP_CELL (WM_APP + 4)      /* lParam: MAKELONG(row, column) of the Combos list */

/* ---------------------------------------------------------------- profiles.cpp: controls profiles (profiles\\<name>.cfg) */
void profile_clean_name(wchar_t *name);              /* no characters a file name cannot have, at most 60 long */
int profile_names(wchar_t names[][64], int max);     /* sorted */
char *profile_load(const wchar_t *name);             /* malloc'ed text or NULL */
int profile_save(const wchar_t *name, const char *text);
int profile_rename(const wchar_t *from, const wchar_t *to);   /* 0 when it fails (the name is taken ...) */
int profile_delete(const wchar_t *name);
int profile_exists(const wchar_t *name);

/* ---------------------------------------------------------------- bindings.cpp: the key / pad mapping of the Controls tab (MAME's cfg\\default.cfg) */
#define NBIND 25                                   /* Player 1 / 2: 4 directions + 6 buttons, Start 1 / 2, Coin 1 / 2, Service */
const wchar_t *bind_label(int row);
void bind_load(void);                               /* reads cfg\\default.cfg of the MAME folder (what was not saved is dropped) */
int bind_autofire_mode(int pluginRuns);   /* the autofire rows of the cfg for this start: NONE (the plugin presses them) or their real codes */
int bind_save(void);                                /* writes the changed rows into it; 1 when nothing to write or written */
int bind_dirty(void);
int bind_is_button(int row);                         /* a row that can have autofire (buttons 1-6 of both players) */
void bind_set_autofire(int row, int on);
int bind_get_autofire(int row);
#ifdef __cplusplus
int bind_autofire_lua(std::string &out);              /* the autofire rows as the entries of a Lua table; their number */
#endif
int bind_migrate(void);                              /* repairs the rows of older builds (pad only, one stick, old trigger codes); 1 when something changed */
void bind_strip_game_cfg(const wchar_t *set);       /* removes the mapped actions from cfg\\<set>.cfg (a copy is kept) so the launcher mapping applies */
void bind_to_text(std::string &out);               /* the changed actions as profile text */
void bind_from_text(const char *text);              /* the mapping of a profile text (actions it does not mention: MAME's defaults) */
void bind_text(int row, wchar_t *key, wchar_t *pad, size_t cap);   /* what the list shows */
int bind_capture_key(HWND owner, int row);          /* a window that waits for a key press; 1 when the row changed */
int bind_capture_pad(HWND owner, int row);          /* ... for a pad / stick button (XInput and DirectInput pads) */
void bind_clear(int row, int col);                  /* col 1: the key, 2: the pad button of the row */
void bind_backup_cfg(void);                          /* copies cfg\\default.cfg to default.cfg.mame-ex-backup (once) */
void bind_reset_all(void);                          /* back to the defaults of MAME: the saved changes of these actions are removed */

/* ---------------------------------------------------------------- filter.cpp: the filter window (xBRZ, FXAA ...) over the MAME window */
void filter_start(DWORD pid);   /* reads the fx* settings; finds the window of the MAME process by itself */
void filter_stop(void);

/* ---------------------------------------------------------------- games.cpp */
enum { GF_NOTWORKING = 1, GF_UNKNOWN = 2 };   /* GF_NOTWORKING: marked so in MAME; GF_UNKNOWN: the installed MAME does not have the set */
typedef struct { int id; wchar_t *title, *info, *set, *parent, *bios; const wchar_t *hw, *maker; int year, flags; } Game;
void dlg_dip_switches(HWND owner, const Game *g);   /* the DIP Switches window of a game (gameextra.cpp) */
const wchar_t *game_hardware(const Game *g);   /* "Sony ZN-1 System", "Namco System 12"... */
int game_year(const Game *g);                  /* 0 = unknown */
extern Game *g_games;
extern int g_ngames;
int games_load(Game **out, int *n);               /* the built-in list of ZN-1 / ZN-2 / System 11 / System 12 games */
void games_ask_mame(Game *g, int n, int force);              /* which sets the installed MAME knows (flags GF_UNKNOWN) and its titles: from mame-ex-listing-cache.cfg, or from MAME when force is set, the cache is missing or mame.exe changed; quiet when MAME is not there */
void games_free(Game *g, int n);
void games_sort_default(Game *g, int n);
void rom_available_bios(const wchar_t *dir);       /* the BIOS folder ("" = none): its sets count as available too (call before rom_available_init) */
int rom_available_init(const wchar_t *dir);       /* builds the set of ROM names of the folder(s) (; separated); returns handle count */
int rom_available(const Game *g);
int rom_missing(const Game *g, wchar_t *out, size_t cap);   /* "a.zip, b.zip" of the ROM sets that are not there; count */
void rom_available_done(void);
int rom_available_stale(const wchar_t *dir);       /* 1 when a ROMs folder changed since rom_available_init (cheap: the time of the folders) */
int create_desktop_icon(const Game *g, wchar_t *err, size_t errCap);
void short_title(const wchar_t *title, int keepVer, wchar_t *out, size_t cap);
void shortcut_name(const Game *g, wchar_t *out, size_t cap);

/* ---------------------------------------------------------------- theme.c: the optional dark theme */
extern int g_dark;
COLORREF th_face(void); COLORREF th_page(void); COLORREF th_text(void); COLORREF th_gray(void); COLORREF th_alt(void);
HBRUSH th_face_brush(void); HBRUSH th_page_brush(void);
void th_init(int dark);
void th_window(HWND h);
void th_control(HWND h, const wchar_t *cls);
HBRUSH th_ctlcolor(UINT msg, HDC dc, HWND ctl, int onPage);   /* NULL in the light theme */
void th_tab(HWND tab);
void th_status(HWND sb);
LRESULT th_menu_msg(HWND h, UINT m, WPARAM w, LPARAM l, int *handled);   /* the menu bar */
void th_menu_line(HWND h);

/* ---------------------------------------------------------------- ui helpers (ui.c) */
extern HINSTANCE g_inst;
extern HFONT g_font;
extern int g_dpi;
extern HICON g_icon, g_iconSm;
void ui_init(HINSTANCE inst);
void wide_from_ascii(const char *s, wchar_t *out, int cap);
extern HWND g_main;
int S(int px);                                     /* scale 96-dpi pixels */
HWND mk(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, DWORD ex, int id);
void set_tip(HWND tipWnd, HWND parent, HWND ctl, const wchar_t *text);
int text_width(HWND h, const wchar_t *t);
int ctl_checked(HWND h);
void ctl_set_checked(HWND h, int on);
int edit_int(HWND h, int lo, int hi);              /* number in an edit box, clamped; lo when empty */
void edit_set_int(HWND h, int v);
void edit_set_text_utf8(HWND h, const char *s);
void edit_get_text_utf8(HWND h, char *out, int cap);
void combo_fill(HWND cb, const wchar_t **items, int n);
LRESULT lv_altrows(LPNMLVCUSTOMDRAW cd);
HWND lv_create(HWND parent, int id, const wchar_t **cols, const int *widths, int ncols);
void lv_get_widths(HWND lv, int *out, int n);          /* in 96 dpi pixels */
void lv_set_widths(HWND lv, const int *w, int n);      /* ignores zeros */
int lv_hit_cell(HWND lv, LPARAM nm, int *row, int *col);
extern int g_modalDepth;
void mame_stop(const PROCESS_INFORMATION *pi);   /* closes the running game the gentle way (ends it after 8 s when it does not answer) */
char *lua_path_u8(const wchar_t *p);   /* a file path for a MAME Lua script (short form when the folder has non-ASCII letters; / separators); free() it */
void msg_box(HWND owner, const wchar_t *title, const wchar_t *text, UINT flags, int *result);

/* modal windows (ui.c): a dialog made of mkat() controls; onok runs when OK is pressed (return 0 to stay open), oncmd for buttons with ids from 1000 */
typedef struct Modal {
    int kind, done, changed;
    HWND owner, dlg, c1, c2;
    char *dst;
    int cap, isX, sel;
    BOOL was[256];
    wchar_t title[120];
    HWND link, link2;   /* clickable texts of the About box */
    HFONT linkFont;
    HICON logo;
    int (*onok)(struct Modal *);
    int (*oncmd)(struct Modal *, int id);
    LRESULT (*onnotify)(struct Modal *, NMHDR *);
    void (*ontimer)(struct Modal *);   /* WM_TIMER 1 of the window (set with SetTimer by the creator) */
    void *user;
} Modal;
HWND make_modal(Modal *md, int cw, int ch);
int lv_min_w(HWND lv, int col);   /* ui.c: the narrowest width of a column of a list (pixels) */
extern int g_lvProg;   /* ui.c: while > 0 the width of a list column may be set below the minimum (hidden columns) */
HWND ui_owner(HWND o);   /* main.c: the window a dialog should belong to (the settings window when it is open) */
HWND mkat(HWND p, const wchar_t *cls, const wchar_t *t, DWORD st, int x, int y, int w, int h, int id);   /* x, y, w, h in 96 dpi pixels */
void run_modal(Modal *md);
typedef struct { const wchar_t *a, *b; } TableRow;   /* b NULL: a title row */
void dlg_table(HWND owner, const wchar_t *title, const wchar_t *c0, const wchar_t *c1, const TableRow *rows, int n, int w0, int w1);   /* a window with a two-column table and an OK button (96 dpi widths) */
int dlg_text(HWND owner, const wchar_t *title, const wchar_t *prompt, wchar_t *text, int cap);   /* one line of text; 1 when OK */

#define APP_VERSION "1.0.0"
#define APP_VERSION_W L"1.0.0"
#define APP_NAME_W L"MAME EX"
void dlg_about(HWND owner);
int dlg_game_settings(HWND owner, const Game *g);   /* gamecfg.c */
void dlg_check_roms(HWND owner, const Game *g);     /* romcheck.c */
int backup_create(const wchar_t *zipPath);          /* backup.c: files written, 0 on failure */
int backup_restore(const wchar_t *zipPath);         /* files restored, -1 when the zip is not a readable backup */
void roms_dir_abs(wchar_t *out, size_t cap);        /* main.c: the ROMs folder as a full path */

/* control ids */
enum {
    ID_SEARCH = 100, ID_CLEARSEARCH, ID_GAMELIST, ID_FAVONLY, ID_ICONBTN, ID_STOPBTN, ID_PLAYBTN, ID_TAB,
    ID_DEFAULTBTN, ID_SAVEBTN, ID_CANCELBTN, ID_OPTBTN, ID_TBFAV, ID_TBFS, ID_TBAVAIL, ID_TBAUDIO, ID_TBSET, ID_TBVIDEO, ID_TBCTL, ID_TBCMB,
    /* menu */
    ID_M_RESCAN = 200, ID_M_OPENMAME, ID_M_OPENROMS, ID_M_CLEARRECENT, ID_M_EXIT, ID_M_PLAY, ID_M_STOP, ID_M_TOGGLEFAV,
    ID_M_SAVE, ID_M_SETAUDIO, ID_M_RESETWIN, ID_M_LAY0, ID_M_LAY1, ID_M_FOCUSSEARCH, ID_M_KEYS, ID_M_ABOUT, ID_M_OPTWIN, ID_COL0, ID_COL1, ID_COL2, ID_COL3, ID_COL4, ID_COL5, ID_COL6, ID_M_THEME0, ID_M_THEME1, ID_M_THEME2, ID_M_FAVONLY, ID_M_AVAIL, ID_M_SHOWSEARCH, ID_M_SHOWFILTERS, ID_M_SHOWTOOLBAR,
    ID_M_GAMECFG = 250, ID_M_PLAYFS, ID_M_PLAYWIN, ID_M_CHECKROMS, ID_M_OPENSNAP, ID_M_BACKUP, ID_M_RESTORE, ID_M_OPENCFG, ID_M_OPENINI, ID_M_MAMEHELP,
    ID_RECENT0 = 600,   /* 8 ids: the recent games (clear of the other ids) */
    /* video page */
    ID_V_MODE = 300, ID_V_FULL, ID_V_RES, ID_V_ROT, ID_V_ASPECT, ID_V_UNEVEN, ID_V_FILTER, ID_V_PRESCALE, ID_V_VSYNC, ID_V_SYNCREF, ID_V_THROTTLE, ID_V_FSKIP,
    ID_V_MAX, ID_V_CHAIN, ID_V_BACKEND, ID_V_CROP, ID_V_HLSL, ID_V_GLSL, ID_V_GLSLF, ID_V_GLSLS, ID_V_INTSCALE, ID_V_FX, ID_V_FXSCALER, ID_V_FXFXAA, ID_V_FXDITHER, ID_V_HLSLSET, ID_X_MOREOPTS,
    /* audio */
    ID_A_MODE = 340, ID_A_RATE, ID_A_VOL, ID_A_LAT,
    /* general */
    ID_S_MAME = 360, ID_S_MAMEBROWSE, ID_S_MAMECLEAR, ID_S_ROMS, ID_S_BROWSE, ID_S_ROMCLEAR, ID_S_AVAIL, ID_S_HIDE, ID_S_LOGS, ID_S_LAYOUT, ID_S_ONSTART, ID_S_THEME, ID_S_BIOS, ID_S_BIOSBROWSE, ID_S_BIOSCLEAR,
    /* controls page */
    ID_C_JOY = 380, ID_C_OVGAME, ID_C_AFRATE, ID_C_PROV, ID_C_MOUSE, ID_C_GUN, ID_C_CONTR, ID_C_DEAD, ID_C_SAT, ID_C_CTRLR, ID_C_OPENCFG, ID_C_LV, ID_C_SETKEY, ID_C_SETPAD, ID_C_CLEAR, ID_C_BDEF, ID_C_PROF, ID_C_PSAVE, ID_C_PSAVEAS, ID_C_PREN, ID_C_PDEL,
    ID_K_STEP = 700, ID_K_CHG, ID_K_LOADCHAR, ID_K_LV, ID_K_CHAR, ID_K_MOVE, ID_K_SEQ, ID_K_PLAY, ID_K_FACE, ID_K_CLEAR, ID_K_DEF, ID_K_LEGEND, ID_K_HOW,
    ID_K_TOK = 720,   /* ... + CMB_NTOK - 1: the sequence buttons */
    /* advanced page */
    ID_X_SKIP = 410, ID_X_SKIPW, ID_X_SAVE, ID_X_CHEAT, ID_X_QUIT, ID_X_EXTRA,
    ID_F_MAKER = 440, ID_F_GENRE, ID_F_REGION, ID_RESETSORT, ID_F_HW
};

#define WM_APP_GAMES (WM_APP + 1)
#define WM_APP_GAMEEXIT (WM_APP + 2)
#define WM_APP_REFILTER (WM_APP + 3)
#define WM_APP_LAYOUT (WM_APP + 9)    /* wParam: 1 the options in a window of their own, 0 in the main window */

/* ---------------------------------------------------------------- toolbar.c */
enum { TI_REFRESH, TI_FAV, TI_PLAY, TI_STOP, TI_FULLSCREEN, TI_SETTINGS, TI_VIDEO, TI_CONTROLS, TI_COMBOS, TI_OK, TI_MISSING, TI_AVAIL, TI_AUDIO, TI_N };
HWND tb_button(HWND parent, int id, int icon);        /* a flat button that shows an icon (owner drawn, theme colours) */
HWND tb_separator(HWND parent);
void tb_set_checked(HWND b, int on);                  /* the pressed look of a switch (Favorites Only) */
int tb_draw(const DRAWITEMSTRUCT *di);                /* WM_DRAWITEM: 1 when it was a toolbar item */
unsigned char *tb_icon_mask(int icon, int variant, int sz);
HIMAGELIST tb_star_images(int sz, COLORREF off, COLORREF on);   /* the favorite stars of the game list */
HIMAGELIST tb_status_images(int sz);   /* the icons of the Status column: 0 = ROM set there (green), 1 = missing (red) */

#endif
