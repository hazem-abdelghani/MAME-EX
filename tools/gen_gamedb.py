#!/usr/bin/env python3
"""Generates src/gamedb_data.inc from the MAME driver sources.

Boards listed (the MAME driver file in brackets):
  Sony ZN-1 / ZN-2 (zn.cpp: the Capcom ZN-1 "coh1000c" / ZN-2 "coh3002c" boards), and the other boards built on the same Sony ZN hardware
  in the same file: Taito FX-1A / FX-1B, Tecmo TPS, Eighting/Raizing, Video System, Acclaim, Atari, Atlus
  Namco System 10 / 11 / 12 (namcos10.cpp, namcos11.cpp, namcos12.cpp)
  SNK Hyper Neo Geo 64 (hng64.cpp)
  Konami GV (konamigv.cpp), Konami System 573 (ksys573.cpp)
  Taito G-Net (taitogn.cpp)

    python3 tools/gen_gamedb.py [--src DIR]        # DIR holds the .cpp files (default: download them from GitHub)

The numbers (#) of the games are kept from the existing gamedb_data.inc, new games get the next free number,
so favorites and per-game settings stay valid when the list is regenerated.
"""
import os, re, sys, urllib.request

BASE = "https://raw.githubusercontent.com/mamedev/mame/master/src/mame/"
# file -> (path in the MAME tree, {BIOS root set: hardware} or a plain hardware name for a file without BIOS root)
BOARDS = [
    ("zn.cpp", "sony/zn.cpp", {
        "coh1000c": "Sony ZN-1 System", "coh3002c": "Sony ZN-2 System",
        "coh1000t": "*taito", "coh1002m": "Tecmo TPS", "coh1002e": "Eighting/Raizing ZN-1",
        "coh1002v": "Video System ZN-1", "coh1000a": "Acclaim ZN-1", "coh1000w": "Atari ZN-1", "coh1001l": "Atlus ZN-1"}),
    ("namcos10.cpp", "namco/namcos10.cpp", "Namco System 10"),
    ("namcos11.cpp", "namco/namcos11.cpp", "Namco System 11"),
    ("namcos12.cpp", "namco/namcos12.cpp", "Namco System 12"),
    ("hng64.cpp", "snk/hng64.cpp", {"hng64": "SNK Hyper Neo Geo 64"}),
    ("konamigv.cpp", "konami/konamigv.cpp", {"konamigv": "Konami GV"}),
    ("ksys573.cpp", "konami/ksys573.cpp", {"sys573": "Konami System 573"}),
    ("taitogn.cpp", "sony/taitogn.cpp", {"coh3002t": "Taito G-Net", "coh1002t": "Taito G-Net"}),
]
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "gamedb_data.inc")

def fetch(name, path, src):
    if src:
        with open(os.path.join(src, name), encoding="utf-8", errors="replace") as f:
            return f.read()
    with urllib.request.urlopen(BASE + path) as r:
        return r.read().decode("utf-8", "replace")

def split_args(s):
    """splits the argument list of a GAME( ... ) macro at the commas outside of quotes"""
    out, cur, q, i = [], "", False, 0
    while i < len(s):
        c = s[i]
        if q:
            cur += c
            if c == "\\" and i + 1 < len(s):
                i += 1
                cur += s[i]
            elif c == '"':
                q = False
        elif c == '"':
            q = True
            cur += c
        elif c == ",":
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
        i += 1
    out.append(cur.strip())
    return out

def unq(s):
    s = s.strip()
    if s.startswith('"') and s.endswith('"'):
        s = s[1:-1]
    return s.replace('\\"', '"').replace("\\\\", "\\")

def macro_args(line, start):
    """the text between the opening parenthesis and its closing one (quotes respected); trailing comments are ignored"""
    i, q = start, False
    while i < len(line):
        c = line[i]
        if q:
            if c == "\\":
                i += 1
            elif c == '"':
                q = False
        elif c == '"':
            q = True
        elif c == ")":
            return line[start:i]
        i += 1
    return None

def games(text):
    res = []
    for line in text.splitlines():
        m = re.match(r"(GAMEL?|COMP|CONS)\(", line)
        if not m:
            continue
        body = macro_args(line, m.end())
        if body is None:
            continue
        a = split_args(body)
        if len(a) < 11:
            continue
        res.append(dict(year=a[0], name=a[1], parent=a[2], machine=a[3], rot=a[7], maker=unq(a[8]), title=unq(a[9]), flags=a[10]))
    return res

def old_ids():
    ids = {}
    try:
        with open(OUT, encoding="utf-8") as f:
            for l in f:
                m = re.match(r'\s*\{(\d+), L"([^"]+)",', l)
                if m:
                    ids[m.group(2)] = int(m.group(1))
    except OSError:
        pass
    return ids

def main():
    src = None
    if "--src" in sys.argv:
        src = sys.argv[sys.argv.index("--src") + 1]
    rows = []
    for name, path, boards in BOARDS:
        gs = games(fetch(name, path, src))
        by = {g["name"]: g for g in gs}
        roots = {g["name"] for g in gs if "MACHINE_IS_BIOS_ROOT" in g["flags"]}

        def root(g):
            """the BIOS root board a game hangs off: follows the clone chain up to the machine whose parent is a root (or nothing)"""
            seen = 0
            while g["parent"] in by and g["parent"] not in roots and seen < 12:
                g = by[g["parent"]]
                seen += 1
            return g["parent"] if g["parent"] in roots else ""

        for g in gs:
            if g["name"] in roots:
                continue
            r = root(g)
            if isinstance(boards, str):
                hw = boards                      # a file without BIOS root: every game belongs to the board
            else:
                if r not in boards:
                    continue                     # a board of the file that is not wanted
                hw = boards[r]
                if hw == "*taito":               # Taito FX-1A and FX-1B differ by machine configuration
                    hw = "Taito FX-1A" if g["machine"] == "coh1000ta" else "Taito FX-1B"
            par = g["parent"] if g["parent"] not in ("0", "") and g["parent"] not in roots else ""   # a parent set can live in another driver file (ptblank2)
            rows.append((hw, g, par, r))
    prev = old_ids()
    nxt = max(prev.values()) + 1 if prev else 1
    lines = []
    seen = set()
    for hw, g, par, bios in rows:
        if g["name"] in seen:
            continue
        seen.add(g["name"])
        if g["name"] in prev:
            gid = prev[g["name"]]
        else:
            gid = nxt
            nxt += 1
        year = int(g["year"]) if g["year"].isdigit() else 0
        flags = 1 if "MACHINE_NOT_WORKING" in g["flags"] else 0
        title = g["title"].replace("\\", "\\\\").replace('"', '\\"')
        maker = g["maker"].replace("\\", "\\\\").replace('"', '\\"')
        lines.append((gid, '    {%d, L"%s", L"%s", L"%s", L"%s", L"%s", L"%s", %d, %d},\n' % (gid, g["name"], par, bios, hw, title, maker, year, flags)))
    lines.sort()
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* generated by tools/gen_gamedb.py from the MAME sources: do not edit by hand.\n")
        f.write("   {#, set, parent set, BIOS set, hardware, title, maker, year, flags (1 = not working in MAME)} */\n")
        for _, l in lines:
            f.write(l)
    print("%d games written to %s" % (len(lines), os.path.relpath(OUT)))

main()
