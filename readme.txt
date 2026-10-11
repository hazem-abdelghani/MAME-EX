MAME EX
=======

MAME EX is an unofficial Windows front-end for MAME (not part of MAME, not
made by the MAME team). It lists 462 games from the 5th-generation
PlayStation-class arcade boards and starts them with MAME.

Boards
------
  Sony ZN-1 / ZN-2 (including Taito FX-1A / FX-1B, Tecmo TPS and Video System
  games), Namco System 10 / 11 / 12, Konami GV and System 573, Taito G-NET,
  SNK Hyper Neo Geo 64.

Package
-------
  MAME-EX.exe          64-bit program
  MAME-EX-32bit.exe    32-bit program
  readme.txt           this file

Requirements
------------
  - Windows 10 or newer (the Filter Window needs Windows 8.1 or newer)
  - a current mame.exe (any recent MAME with Lua plugin support, as in the
    official builds)
  - the ROM sets (zip files), the BIOS set of the board (for example
    coh1000c.zip, coh3002c.zip, hng64.zip, sys573.zip, konamigv.zip) and the
    parent set of a clone

Installation
------------
  1. Copy MAME-EX.exe (or MAME-EX-32bit.exe) next to mame.exe, or into a
     folder of its own and choose mame.exe on the General tab.
  2. Start it. Set the ROMs folder on the General tab (empty = the roms folder
     of MAME). If your BIOS zip files are in another folder, set the BIOS
     Folder as well (empty = they are in the ROMs folder).
  3. Double-click a game.

The launcher never edits mame.ini: everything is passed to MAME on the command
line, so a game started from the launcher has exactly the settings you see.

Main features
-------------
  Game list      Search, favorites, region / maker / genre filters, sortable
                 and hideable columns, recent games, light / dark theme.
  Per-game       Video mode, window, resolution, rotation, sound, cheats,
  settings       BIOS, DIP switches, extra arguments.
  Video          Video mode (BGFX / Direct3D / OpenGL / GDI), HLSL Settings
                 with presets, prescale, bilinear filter, aspect, V-Sync,
                 frame skip, brightness / contrast / gamma, artwork crop.
  Controls       Key and pad mapping written into MAME's cfg\default.cfg,
                 profiles, an Autofire column (buttons 1-6) with speed,
                 joystick provider, dead zone, light gun options.
  Combos         Move sequences on a trigger key or pad button (Lua plugin).
  Other          Check ROM Set (zip check plus MAME's own -verifyroms),
                 backup and restore, rewind, auto save, recording, All MAME
                 Options, desktop icons, frontend command line.

Default keys
------------
  Player 1   Up / Down / Left / Right: W S A D
             Buttons 1-6: numpad 4 5 6 1 2 3 (NumLock on)
             Start: Enter    Coin: Right Shift
  Player 2   Up / Down / Left / Right: arrow keys
             Buttons 1-6: U I O J K L
             Start: Y        Coin: H
  Service    F2
  Pad        D-pad and left stick for directions; buttons 1-6 = X Y RB A B RT;
             Start = Start, Coin = Back / Select.
  Change them on the Controls tab (double-click a cell, or right-click a row:
  Set Key / Set Pad / Clear / Autofire). OK writes them into MAME's own
  cfg\default.cfg. At the very first start the launcher's defaults are used;
  the original default.cfg is kept once as default.cfg.mame-ex-backup.

Frontend command line
---------------------
  MAME-EX.exe --game <set name or ROM zip> [--fullscreen | --windowed]

  The call waits until MAME is closed. Exit codes: 0 played, 2 unknown game,
  3 ROM files missing, 4 mame.exe could not be started, 5 no game given.
  Use it with LaunchBox, Pegasus, RetroBat, Playnite, Steam ROM Manager or
  Attract-Mode.

Files the launcher creates (next to MAME-EX.exe)
------------------------------------------------
  mame-ex-settings.cfg        launcher settings
  mame-ex-games.cfg           per-game settings
  mame-ex-combos.cfg          combos
  mame-ex-autofire.cfg        autofire rows
  mame-ex-listing-cache.cfg   which sets the installed MAME knows
  mame-ex-padprobe.txt        names of your pad's controls, asked from MAME
  profiles\                   saved key / pad mappings
  mame-ex-plugins\            the Lua plugins written for MAME (combos /
                              autofire, pad probe, input log); safe to delete
  logs\                       log files, only with Enable Logs (General tab,
                              off by default)

  Everything can be deleted: the launcher recreates what it needs. Use
  File > Backup Settings to save your settings, profiles and MAME's cfg / nvram / sta /
  ini folders.

Notes
-----
  - Stop asks MAME to close its window (so nvram, cfg and the autosave state
    are written) and ends it only after 8 seconds without an answer.
  - Only one launcher runs per folder.
  - Autofire needs the launcher's Lua plugin. Without it the autofire buttons
    work as normal buttons, without the repeat.
  - The Filter Window (xBRZ / FXAA, experimental) copies the picture of the
    MAME window: do not use MAME's own fullscreen (Alt+Enter) with it.
  - An internal resolution above the console's own cannot be offered: MAME
    draws these games with a software GPU at the console's resolution.

Troubleshooting
---------------
  - "mame.exe was not found": choose it on the General tab (MAME Program), or
    put the launcher next to mame.exe.
  - A game is marked red / cannot start: a ROM or BIOS zip is missing. Use
    Game > Check ROM Set to see which file.
  - A key or pad button does nothing: turn on Enable Logs (General tab), start
    the game once, and look at logs\mame-ex-input.log and
    logs\mame-ex-launch.log.

Building from source
--------------------
  python3 -m pip install ziglang
  sh build.sh          (Linux / macOS / Git Bash)  or  build.bat (Windows)
  A mingw-w64 toolchain works as well (see the comment in build.sh).

License
-------
  GNU General Public License v3 (see LICENSE). MAME has its own licenses
  (GPL / BSD); it is not part of this package.
