#!/bin/sh
# Builds MAME-EX.exe (64-bit) and MAME-EX-32bit.exe:   sh build.sh
#
# Uses Zig's bundled C++ compiler / resource compiler (python3 -m pip install ziglang), no Windows needed.
# With a mingw-w64 toolchain instead:
#   cd src && windres app.rc -O coff -o app.o
#   x86_64-w64-mingw32-g++ -std=c++17 -Os -s -municode -mwindows -static -o ../MAME-EX.exe $SRC app.o $LIBS
set -e
cd "$(dirname "$0")/src"
SRC="main.cpp ui.cpp util.cpp settings.cpp games.cpp mame.cpp gamecfg.cpp filter.cpp bindings.cpp profiles.cpp padprobe.cpp hlsl.cpp moreopts.cpp gameextra.cpp combos.cpp combo_data.cpp icon_data.cpp tips.cpp zipio.cpp romcheck.cpp classify.cpp backup.cpp theme.cpp toolbar.cpp"
LIBS="-luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -luxtheme -lwinmm -ladvapi32 -ld3d11 -ldxgi -ldxguid"
ZIG="${ZIG:-python3 -m ziglang}"
FLAGS="-std=c++17 -Os -s -municode -Wl,--subsystem,windows -Wall -Wno-unused -Wno-parentheses -Wno-nullability-completeness"
$ZIG rc /fo app.res app.rc
$ZIG c++ -target x86_64-windows-gnu $FLAGS -o ../MAME-EX.exe $SRC app.res $LIBS
$ZIG c++ -target x86-windows-gnu $FLAGS -o ../MAME-EX-32bit.exe $SRC app.res $LIBS
rm -f app.res
ls -l ../MAME-EX.exe ../MAME-EX-32bit.exe
