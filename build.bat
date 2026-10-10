@echo off
rem Builds MAME-EX.exe (64-bit) and MAME-EX-32bit.exe on Windows with Zig:   build.bat
rem Needs Python and "python -m pip install ziglang" (or set ZIG to another zig command, e.g. set ZIG=zig).
setlocal
cd /d "%~dp0src"
if "%ZIG%"=="" set ZIG=python -m ziglang
set SRC=main.cpp ui.cpp util.cpp settings.cpp games.cpp mame.cpp gamecfg.cpp filter.cpp bindings.cpp profiles.cpp padprobe.cpp hlsl.cpp moreopts.cpp gameextra.cpp combos.cpp combo_data.cpp icon_data.cpp tips.cpp zipio.cpp romcheck.cpp classify.cpp backup.cpp theme.cpp toolbar.cpp
set LIBS=-luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -luxtheme -lwinmm -ladvapi32
set FLAGS=-std=c++17 -Os -s -municode -Wl,--subsystem,windows -Wall -Wno-unused -Wno-parentheses -Wno-nullability-completeness
%ZIG% rc /fo app.res app.rc || goto :fail
%ZIG% c++ -target x86_64-windows-gnu %FLAGS% -o ..\MAME-EX.exe %SRC% app.res %LIBS% || goto :fail
%ZIG% c++ -target x86-windows-gnu %FLAGS% -o ..\MAME-EX-32bit.exe %SRC% app.res %LIBS% || goto :fail
del app.res
echo.
echo Done: MAME-EX.exe and MAME-EX-32bit.exe
exit /b 0
:fail
echo.
echo Build failed.
exit /b 1
