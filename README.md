![MAME EX](images/MAME_EX_Logo.png)
# MAME EX

<img src="images/logo.png" width="96" alt="MAME EX logo">

**MAME EX** is an unofficial Windows front-end for **[MAME](https://github.com/mamedev/mame)** (not part of MAME, not made by the MAME team). It lists 462 games from the 5th-generation PlayStation-class arcade boards and starts them with MAME.

- **Boards:** Sony ZN-1 / ZN-2 (including Taito FX-1A / FX-1B, Tecmo TPS and Video System games), Namco System 10 / 11 / 12, Konami GV and System 573, Taito G-NET, SNK Hyper Neo Geo 64
- **Game list:** search, favorites, region / maker / genre filters, per-game settings (BIOS, DIP switches, video, sound, cheats)
- **Controls:** key and pad mapping with profiles and an autofire column
- **Combos:** move sequences on a trigger key or pad button
- **Video:** HLSL settings with presets, BGFX, Direct3D, OpenGL
- **Other:** BIOS folder, Check ROM Set, backup and restore, command line for LaunchBox, Pegasus, RetroBat and Playnite

Written in C++17 with the Win32 API only. Builds with Zig or mingw-w64. GPL v3.

Only the 5th-generation PlayStation-class boards below are listed (462 games); the games come straight from the MAME driver sources:

| Board | MAME driver | BIOS set | Games |
|---|---|---|---|
| Sony ZN-1 (Capcom ZN-1) | `zn.cpp` | `coh1000c` | Street Fighter EX / EX Plus, Star Gladiator, Battle Arena Toshinden 2, Gallop Racer |
| Sony ZN-2 (Capcom ZN-2) | `zn.cpp` | `coh3002c` | Street Fighter EX2 / EX2 Plus, Rival Schools, Plasma Sword, Tech Romancer, Strider 2, Tetris The Grand Master ... |
| Taito FX-1A / FX-1B | `zn.cpp` | `coh1000t` | Psychic Force, Super Football Champ, Magical Date, RayStorm, G-Darius, Fighters' Impact |
| Tecmo TPS | `zn.cpp` | `coh1002m` | Gallop Racer 2 / 3, Dead Or Alive ++, Cool Boarders Arcade Jam, Shanghai Matekibuyuu, Monster Farm Jump ... |
| Eighting/Raizing, Video System, Acclaim, Atari, Atlus ZN-1 | `zn.cpp` | `coh1002e` `coh1002v` `coh1000a` `coh1000w` `coh1001l` | Bloody Roar 1 / 2, Beastorizer, Aero Fighters Special, NBA Jam Extreme, Judge Dredd, Primal Rage 2, Heaven's Gate |
| Namco System 10 | `namcos10.cpp` | – | Mr. Driller 2, Gamshara, Point Blank 3, Gahaha Ippatsudou, Puzz Ball, Knock Down 2001 ... |
| Namco System 11 | `namcos11.cpp` | – | Tekken 1 / 2, Soul Edge, Xevious 3D/G, Dunk Mania, Prime Goal EX, Point Blank 2 (S11 sets) ... |
| Namco System 12 | `namcos12.cpp` | – | Tekken 3, Tekken Tag Tournament, Soul Calibur, Ehrgeiz, Point Blank 2, Mr. Driller, Ghoul Panic, Golgo 13 ... |
| SNK Hyper Neo Geo 64 | `hng64.cpp` | `hng64` | Samurai Shodown 64 / Warriors Rage, Fatal Fury: Wild Ambition, Buriki One, Roads Edge, Xtreme Rally, Beast Busters: Second Nightmare |
| Konami GV | `konamigv.cpp` | `konamigv` | Powerful Pro Yakyuu '96, Hyper Athlete, Beat the Champ, Dead Eye, Wedding Rhapsody, Tokimeki Memorial, Nagano Winter Olympics '98, The Simpsons Bowling |
| Konami System 573 | `ksys573.cpp` | `sys573` | Dance Dance Revolution (all mixes), Dancing Stage, Dance Maniax, Guitar Freaks, DrumMania, Bass Angler ... (155 sets) |
| Taito G-Net | `taitogn.cpp` | `coh3002t` `coh1002t` | RayCrisis, Chaos Heat, Super Puzzle Bobble, RC De Go, Psyvariar, Shikigami no Shiro, mahjong games ... |

## Installation

1. Get a current `mame.exe` (any recent MAME) and the ROM sets (the zip files, plus the BIOS set of the board (`coh1000c.zip`, `coh3002c.zip`, `hng64.zip`, `sys573.zip`, `konamigv.zip` ... see the table) and the parent set of a clone).
2. Copy `MAME-EX.exe` (64-bit) or `MAME-EX-32bit.exe` next to `mame.exe` (or into a folder, and choose `mame.exe` on the *General* tab).
3. Start it, set the **ROMs folder** (empty = the `roms` folder of MAME) and double-click a game.

The launcher keeps its own files next to itself: `mame-ex-settings.cfg` (all launcher settings) and `mame-ex-games.cfg` (per-game settings).
It never edits `mame.ini`: everything is passed to MAME on the command line (so a game started from the launcher has exactly the settings you see).

## MAME EX features

- **Game list:** 462 games of the boards above, from a table generated from the MAME driver sources (`tools/gen_gamedb.py`). `mame -listfull` tells which sets the installed MAME really has (the others are marked *not in this MAME* and cannot be started); the answer is kept in `mame-ex-listing-cache.cfg`, so MAME is not started at every launch, only when `mame.exe` changes or you press *Rescan Games* (F5). Search, favorites, region / maker / genre / hardware filters, sortable and hideable columns, recent games.
- **MAME's own video and sound:** Video Mode (BGFX / Direct3D / OpenGL / GDI), BGFX screen effects, **HLSL Settings** (scanlines, shadow mask, curvature, bloom, chroma, LUT, NTSC, presets), prescale, bilinear filter, aspect, V-Sync, frame skip, artwork crop, Display (brightness, contrast, gamma) and sound (output module, sample rate, volume, latency).
- **Controls tab:** keyboard and pad mapping written into MAME's `cfg\default.cfg`, profiles, an **Autofire** column with speed, joystick provider, dead zone and saturation, light gun device / provider, controller files (`-ctrlr`).
- **Combos tab:** move sequences on a trigger key or pad button, run by a Lua plugin of the launcher.
- **Per-game settings:** video mode, window, resolution, rotation, sound, cheats, extra arguments, **BIOS** and **DIP Switches**.
- **All MAME Options:** every other command line option that matters for these boards, with MAME's description, a search box and the default of each. A **BIOS Folder** next to the ROMs folder is also supported.
- **Check ROM Set:** zip directory + CRC check, plus MAME's own `-verifyroms`, over every ROMs folder and the BIOS folder.
- **Cheats** (`-cheat`), **Auto Save / Restore State**, **Rewind**, recording and input playback (Capture And Speed).
- **Backup / Restore** of the launcher files and of the MAME `cfg`, `nvram`, `sta` and `ini` folders.
- **Desktop icons** and a **frontend command line** (below), so LaunchBox, Pegasus, RetroBat, Playnite and similar can start games.
- **Enable Logs** (off by default): all log files go into a `logs` folder beside the launcher.

**Behaviour notes:** *Stop* asks MAME to close its window (so nvram, cfg and the autosave state are written) and ends it only after 8 seconds without an answer. The ROM sets are read once and again only when a ROMs folder changes (its time stamp), on F5, or when the ROMs folder box changes. Only one launcher runs per folder (a second start brings the first to the front). Window, columns and choices are saved on logoff / shutdown too. Autofire needs the launcher's Lua plugin; when it does not run for a start (the filter is off for the game, or it could not be written) the autofire buttons get their real keys back for that start, without the repeat. Key sequences with several codes (`KEYCODE_LALT KEYCODE_ENTER`, `... NOT ...`) made in MAME's own menu are kept as they are.

*OK* saves the settings; *Cancel*, or closing the settings window or the program with the X of the title bar, drops what was changed on the tabs and was not confirmed with *OK*.

The pad capture asks MAME what it calls the controls of your pad: the first time it runs MAME for a second (hidden, the game pong, `-video none`, with a tiny Lua plugin) and keeps the answer in `mame-ex-padprobe.txt`. So A / B / X / Y, the D-pad and LT / RT are written with the codes of *your* MAME version.

With **Enable Logs** (General tab, off by default) every game start also loads a small logging plugin: `logs\mame-ex-input.log` (in the `logs` folder beside the launcher) lists MAME's input devices, the sequence MAME really uses for each Player 1 / 2 action, and every press and release while you play (which part of the sequence — key, D-pad, stick, button — caused it). `mame-ex-launch.log` says whether a game-specific `cfg\<game>.cfg` exists (it wins over `default.cfg`). Send these two files if a binding does not work.

**At the very first start** (no `mame-ex-settings.cfg` yet) the key / pad mapping starts with the launcher's defaults, whatever MAME's `cfg\default.cfg` held for those actions before (the original file is kept once as `default.cfg.mame-ex-backup`). **Mapping Overrides MAME's Game Settings** (Controls tab, on by default): MAME saves keys changed in its own Tab menu in `cfg\<game>.cfg`, which wins over `cfg\default.cfg`; before each start the launcher removes the mapped actions from that file (the original is kept as `<game>.cfg.mame-ex-backup`).

**Default keys** (Defaults button, and at the first start): Player 1 W A S D, buttons 1–6 on the numpad 4 5 6 1 2 3, Start Enter, Coin Right Shift; Player 2 the arrow keys, buttons U I O J K L, Start Y, Coin H; Service F2. The pad side of each action is the D-pad and the left stick for directions and the buttons X, Y, RB, A, B, RT for buttons 1–6, Start / Back for Start / Coin. For a DirectInput pad (when MAME does not list it as "XInput") the same places are used: BUTTON3, BUTTON4, BUTTON6, BUTTON1, BUTTON2, the Z axis for RT, and BUTTON8 / BUTTON7 for Start / Back (the numbering of an Xbox-type DirectInput pad; other pads: use Set Pad). (Numpad keys need NumLock on.)

**HLSL Settings…** (Video tab, with the Direct3D video mode and HLSL ticked): the parameters of MAME's HLSL screen effects (the shaders in MAME's `hlsl` folder: bloom, chroma, scanlines, shadow mask ...): scanlines, shadow mask, screen curvature, bloom, colour, phosphor and NTSC values, with a few presets. Only values that differ from MAME's defaults are passed on the command line.

**All MAME Options…** (General tab): every other command line option of MAME (about 45, only those that matter for these boards: search paths, gamma / brightness / contrast, screenshots, recording, input recording / playback, speed, coin and joystick options, light gun and pedal / paddle devices, priority ...; vector, debugger, network, MIDI, computer-only and OpenGL quirk options are left out) with MAME's own description, a search box and the default of each. The list is generated from the MAME sources by `tools/gen_options.py` (options the launcher already has controls for, and the HLSL ones, are left out). Only values that differ from MAME's defaults are passed, so an option an older MAME does not know only matters when you change it. Options that belong to a single game (for example `-bios` of Hyper Neo Geo 64 or Taito G-NET) are better put in that game's *Extra Arguments* (Game > Game Settings): a BIOS name that another game does not have makes MAME stop.

**More controls, from the list of options MAME has and the launcher did not:**
- *Controls tab:* an **Autofire** column for buttons 1-6 of both players (click the cell, or right-click the row) and an **Autofire Speed** box (10 / 15 / 20 / 30 presses per second); **Light Gun Device** and **Light Gun Provider** (`-lightgun_device`, `-lightgunprovider`). Autofire rows are NONE in MAME's `default.cfg`; the launcher's Lua plugin presses the button while the key / pad button of the row is held (the real codes are kept in `mame-ex-autofire.cfg`, and in profiles).
- *Video tab:* **Display** (Brightness, Contrast, Gamma, Aspect Ratio) and **BGFX Extras** (shadow mask, LUT).
- *General tab:* **Capture And Speed** (Speed, Rewind, Snapshot Size, record video / audio / input, play input, load state).
- *Game > Game Settings > Other:* **BIOS** (the BIOS versions MAME lists for that game, passed as `-bios`) and **DIP Switches…** (every switch of the game with its values, written into the game's own `cfg\<game>.cfg`, where MAME keeps them; the game must not be running).
- *General tab > All MAME Options…:* everything else.

## Frontend command line

MAME EX can start a game without showing its window, so other front-ends (LaunchBox, Pegasus, RetroBat, Playnite, Steam ROM Manager, Attract-Mode) can use it as the program that runs a game:

```
MAME-EX.exe --game <set name or ROM zip> [--fullscreen | --windowed]
```

- `--game` takes the MAME set name (for example `sfex2`) or the path of the ROM zip. The game starts with the global settings and the game's own settings of the launcher.
- `--fullscreen` / `--windowed` override the video setting for this start.
- The call waits until MAME is closed, so the front-end knows when the game ends.
- Exit codes: `0` played, `2` unknown game, `3` ROM files missing, `4` `mame.exe` could not be started, `5` no game given. With **Enable Logs** on, the reason is in `logs\mame-ex-launch.log`.
- Desktop icons made by the launcher (Game menu) start `mame.exe` directly with the game's settings.

## Combos

The *Combos* tab (20 rows) gives a trigger (a key or a pad button, set like the Controls tab) to a move sequence. The sequences are typed or built in words (`Quarter-circle Forward + Heavy Punch`, `QCF + HP`, `236+HP`), *Load Character* fills the rows with the move lists of the Street Fighter EX2 Plus characters, charge moves (`Hold`) and the player side (left / right) are supported, and *Plays For* picks Player 1 or 2.

(MAME's own `plugins` folder stays in `-pluginspath`: its boot.lua is what loads the plugin.) How it runs with MAME: before a game starts the launcher writes a small MAME **Lua plugin** (`mame-ex-plugins\mame-ex-combos-plugin\mamex_combos`, rewritten at every start; `-plugins -pluginspath ... -plugin mamex_combos` are added to the command line). The plugin watches the trigger and presses the buttons of the player step by step. It needs a MAME build with Lua plugin support (the official builds have it) and the ports named `P1 Button 1` ... `P1 Up` ... (all ZN / System 11 / 12 games). The first nine rows start with the keys 1 to 9 as triggers (MAME also uses 1 / 2 / 5 / 6 for Start and Coin, so those rows act together with them; change the key if that is in the way). Rows without a trigger or without a sequence are ignored; with none set up no plugin is used. The plugin writes `logs\mame-ex-combos.log` beside the launcher (what it found and when a combo started; look there if a combo does nothing). The settings are in `mame-ex-combos.cfg` (also in the backup).

## Key and Pad Mapping

The *Controls* tab has a list of the actions of the games (Player 1 / 2 directions and buttons 1–6, Start, Coin, Service) with a *Keyboard* and a *Pad / Joystick* column (right-click a row for Set Key / Set Pad / Clear Key / Clear Pad).
Double-click a cell (or choose a row and press *Set Key…* / *Set Pad…*) and press the key or the button: XInput (Xbox) pads, other pads and arcade sticks are found; for a direction, move the D-pad or stick.
*OK* writes the changes into MAME's own `cfg\default.cfg` (only these actions; everything else in the file stays), so they hold for all games. An action you never changed keeps MAME's defaults and its automatic pad mapping. The first change of an action writes MAME's default for it (default key; D-pad and left stick for directions; the buttons of an Xbox pad), so keyboard and pad keep working together; choosing a D-pad or a stick for a direction keeps the other one.

**Profiles:** the *Profile* box above the list holds mappings you saved under a name (`profiles\<name>.cfg` next to the launcher, included in the backup). Choosing one loads it into the list (press *OK* to write it to MAME); *Save* stores the list in the chosen profile, *Save As…* in a new one, *Rename…* and *Delete* manage them. A profile holds the actions you changed, so actions it does not mention go back to MAME's defaults.
A game's own `cfg\<game>.cfg` (made when you change keys inside MAME with Tab) still wins over it. `tools/test_bindings.cpp` tests the file reading / writing on any host.

## Filter Window (Experimental): xBRZ, FXAA, Dither Smoothing

MAME has no xBRZ / FXAA, and an internal resolution above the console's own cannot be offered: MAME draws these games with a software GPU at the console's own resolution.
What the launcher can do is filter the finished picture. With *Video > Filter Window* ticked:

- MAME is started in a plain window of the game's own picture size (no aspect correction, no smoothing; the launcher adds `-window -nomaximize -nokeepaspect -nofilter -prescale 1 -intscalex 0 -intscaley 0 -resolution auto`).
- A Direct3D 11 window (`filter.cpp`) copies the picture of that window every frame (`PrintWindow`), runs the pixel shaders of the launcher over it – **xBRZ** scaler (or a smooth bicubic / sharp bilinear one for 3D games), **FXAA**, **dither smoothing** – and shows the result over the whole screen: 4:3 with black bars when *Keep Aspect Ratio* is ticked, otherwise stretched.
- The window is click-through and never active, so keyboard and mouse go to MAME. It is shown only while MAME has the focus. It closes with MAME.

Limits: about one frame of extra delay; the picture is copied from the MAME window, so MAME's own fullscreen (Alt+Enter) must not be used; desktop icons start MAME without the filter; it needs Windows 8.1 or newer and a Direct3D 10/11 graphics card. Internal resolution above the console's cannot be offered.

## Building

The sources are in `src/` (C++17, Win32 API only, no framework).

```
python3 -m pip install ziglang
sh build.sh        # Linux / macOS / Git Bash:  MAME-EX.exe and MAME-EX-32bit.exe
build.bat          # Windows Command Prompt
```
A mingw-w64 toolchain works as well (see the comment in `build.sh`).

### Updating the game list when MAME adds games

```
python3 tools/gen_gamedb.py        # downloads the driver files of the table above from GitHub and rewrites src/gamedb_data.inc
```
The numbers (#) of the games already listed are kept, so favorites and per-game settings stay valid.

## License

GNU General Public License v3 (see `LICENSE`). MAME has its own licenses (GPL / BSD); it is not part of this package.

## Credits

MAME EX is by Hazem Abdelghani. MAME belongs to the MAME team; this project only starts it.


## Settings notes

- **Enable Logs** (General tab, off by default) controls every log file of the launcher, all kept in the `logs` folder beside it: `mame-ex-launch.log`, `mame-ex-output.log`, `mame-ex-combos.log`, `mame-ex-input.log`. Off: none is written (MAME's console output goes to a temporary file in the Windows temp folder, only so the launcher can show why a game failed).
- **BIOS Folder** (General tab, below ROMs Folder): the BIOS zip files can live in a folder of their own. It is added to MAME's `-rompath` and counted by the Status column and Check ROM Set. Empty: the BIOS sets are in the ROMs folder.
- At the first start the search bar, the filter bar and the classic layout (settings as tabs in the main window) are off; View and Options > Settings switch them on.
- The Lua plugins the launcher writes (combos / autofire, the pad probe, the input log) are in the folder `mame-ex-plugins` beside the launcher, one sub-folder each (`mame-ex-combos-plugin`, `mame-ex-probe-plugin`, `mame-ex-diag-plugin`). They are rewritten when needed and can be deleted at any time.
