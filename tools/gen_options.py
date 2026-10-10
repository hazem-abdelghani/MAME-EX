#!/usr/bin/env python3
"""Builds src/mameopts_data.inc: every command line option of MAME (core, OSD and Windows ones) with its type, default, range and description,
read from the MAME sources (emuopts.cpp, osdobj_common.cpp, winopts.cpp and the headers with the OPTION_* names).
The options the launcher already has controls for (found in src/mame.cpp), the HLSL ones (src/hlsl.cpp has their own window) and a few the launcher
manages itself are left out.
Usage: python tools/gen_options.py [folder with the six source files]   (downloaded from GitHub when no folder is given)"""
import os, re, sys, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = 'https://raw.githubusercontent.com/mamedev/mame/mame0289/src/'
FILES = ['emu/emuopts.h', 'osd/modules/lib/osdobj_common.h', 'osd/windows/winopts.h',
         'emu/emuopts.cpp', 'osd/modules/lib/osdobj_common.cpp', 'osd/windows/winopts.cpp']

def load(name, folder):
    base = os.path.basename(name)
    if folder:
        return open(os.path.join(folder, base), encoding='utf-8', errors='replace').read()
    return urllib.request.urlopen(BASE + name, timeout=60).read().decode('utf-8', 'replace')

folder = sys.argv[1] if len(sys.argv) > 1 else None
src = {f: load(f, folder) for f in FILES}
macros = {}
for f in FILES[:3]:
    for m in re.finditer(r'#define\s+(\w+)\s+"([^"]*)"', src[f]):
        macros[m.group(1)] = m.group(2)

macros.update({'OSD_FONT_PROVIDER': 'fontprovider', 'OSD_JOYSTICKINPUT_PROVIDER': 'joystickprovider', 'OSD_KEYBOARDINPUT_PROVIDER': 'keyboardprovider',   # defined in headers that are not read here
               'OSD_LIGHTGUNINPUT_PROVIDER': 'lightgunprovider', 'OSD_MONITOR_PROVIDER': 'monitorprovider', 'OSD_MOUSEINPUT_PROVIDER': 'mouseprovider', 'OSD_OUTPUT_PROVIDER': 'outputprovider'})

def lit(tok):
    tok = tok.strip()
    if tok == 'nullptr': return None
    if tok.startswith('"'): return bytes(tok[1:-1], 'utf-8').decode('unicode_escape')
    return macros.get(tok, tok)

entry = re.compile(r'\{\s*(nullptr|\w+)\s*(?:"([^"]*)")?\s*,\s*(nullptr|"(?:[^"\\]|\\.)*"|\w+)\s*,\s*core_options::option_type::(\w+)\s*,\s*(nullptr|"(?:[^"\\]|\\.)*")\s*\}')

used = set()
mc = open(os.path.join(HERE, '..', 'src', 'mame.cpp'), encoding='utf-8').read()
used |= set(re.findall(r'L"-(?:no)?([a-z_0-9]+)"', mc))
used |= set(re.findall(r'arg_flag\(a, L"([a-z_0-9]+)"', mc))
used |= set(re.findall(r'arg_add\(a, L"-?([a-z_0-9]+)"', mc))
hl = open(os.path.join(HERE, '..', 'src', 'hlsl.cpp'), encoding='utf-8').read()
hlsl = set(re.findall(r'\{L"[^"]*", L"[^"]*", "(\w+)"', hl))
skip = {'writeconfig', 'plugin', 'plugins', 'pluginspath', 'noplugin', 'cfg_directory', 'hlslpath', 'hlsl_write', 'hlsl_snap_width', 'hlsl_snap_height',
        'readconfig', 'rompath', 'verbose'}   # verbose / readconfig change how MAME reports; rompath is the ROMs folder box; the plugin ones belong to the launcher
# Options that do nothing for the boards of this launcher (raster 3D arcade games of the PlayStation generation: Sony ZN-1/ZN-2 and the boards built on
# them, Namco System 10/11/12, Konami GV and System 573, Taito G-NET, SNK Hyper Neo Geo 64): vector screens, several monitors, computers (keyboards,
# software lists, slots), the debugger and scripts, network play, MIDI, the web server, and the quirks of the OpenGL / BGFX modules.
irrelevant = set("""
homepath hashpath samplepath fontpath languagepath swpath comment_directory share_directory
burnin snapview mngwrite refreshspeed sleep
unevenstretchx unevenstretchy autostretchxy intoverscan autoror autorol fallback_artwork override_artwork artwork_font pause_brightness
beam_width_min beam_width_max beam_dot_size beam_intensity_weight flicker vector_beam_smooth vector_length_scale vector_length_ratio bgfx_vectorcrt samples
multikeyboard multimouse steadykey ui_active natural adstick_device dial_device trackball_device positional_device mouse_device
log oslog debug update_in_pause debugscript debuglog debugger debugger_host debugger_port debugger_font debugger_font_size watchdog bench
comm_localhost comm_localport comm_remotehost comm_remoteport comm_framesync networkprovider midiprovider
drc drc_rwx drc_use_c drc_log_uml drc_log_native bios uifont ui ui_mouse language rtc
autoboot_command autoboot_delay autoboot_script console http http_port http_root uimodekey controller_map
fontprovider outputprovider mouseprovider numscreens monitorprovider screen view switchres
gl_forcepow2texture gl_notexturerect gl_vbo gl_pbo glsl_shader_mame glsl_shader_screen
bgfx_path bgfx_debug bgfx_avi_name profile attach_window ui_lut_texture ui_lut_enable
""".split())
skip |= irrelevant
rows = []
for f in FILES[3:]:
    group = ''
    for m in entry.finditer(src[f]):
        nm, alias, dflt, kind, desc = m.groups()
        if kind == 'HEADER':
            group = lit(desc).replace('OPTIONS', '').strip().title().replace('Osd ', 'OSD ').replace('Hlsl', 'HLSL').replace('Ntsc', 'NTSC')
            continue
        if nm == 'nullptr': continue
        name = lit(nm + ((' "' + alias + '"') if False else '')) if nm != 'nullptr' else None
        if not name: continue
        rng = re.search(r'\(([-0-9.]+)-([-0-9.]+)\)', alias or '')
        d = lit(dflt)
        if name in used or name in hlsl or name in skip: continue
        if kind in ('COMMAND', 'HEADER'): continue
        k = {'BOOLEAN': 'B', 'INTEGER': 'I', 'FLOAT': 'F', 'STRING': 'S', 'PATH': 'P', 'MULTIPATH': 'P', 'FILE': 'P'}.get(kind)
        if not k: continue
        if not desc or lit(desc) is None: continue
        if not group or 'COMMAND' in group.upper(): continue
        rows.append((group, name, k, d if d is not None else '', rng.group(1) if rng else '', rng.group(2) if rng else '', lit(desc)))

seen = set()
out = ['/* Generated by tools/gen_options.py from the MAME sources. DO NOT EDIT. group, name, kind (B bool, I int, F float, S text, P path), default, range, description */']
for r in rows:
    if r[1] in seen: continue
    seen.add(r[1])
    def c(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
    out.append('    {%s, %s, \'%s\', %s, %s, %s, %s},' % (c(r[0]), c(r[1]), r[2], c(r[3]), c(r[4]), c(r[5]), c(r[6])))
open(os.path.join(HERE, '..', 'src', 'mameopts_data.inc'), 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print(len(seen), 'options;', len(used), 'used by the launcher;', len(hlsl), 'in the HLSL window')
