/* What MAME calls the controls of the pads on this PC: a short MAME run (-video none, the game pong, one second) with a small Lua plugin that
   writes the joystick devices and their items (name, token, value at rest) to mame-ex-padprobe.txt. The pad capture uses it to write the codes
   MAME itself uses (the buttons A / B / X / Y, the D-pad, LT / RT ...), whatever the version of MAME, instead of guessing them. */
#include "common.h"
#include <string>
#include <vector>

typedef struct { std::string name, token; int cur; } PItem;
typedef struct { int index; std::string name; std::vector<PItem> items; } PDev;
static std::vector<PDev> g_pdev;
static void parse_probe(const char *txt);
static int g_probed, g_cacheTried;

static void load_cache(void) {   /* the result of an earlier run (mame-ex-padprobe.txt), for the names shown in the list */
    wchar_t outp[560];
    char *txt;
    if (g_cacheTried) return;
    g_cacheTried = 1;
    path_join(outp, 560, g_root, L"mame-ex-padprobe.txt");
    txt = read_file(outp, NULL);
    if (txt) { if (strstr(txt, "END")) parse_probe(txt); free(txt); }
}

static const char *LUA =
"local exports = {name = 'mamex_probe', version = '1.0.0', description = 'MAME EX pad probe', license = 'BSD-3-Clause', author = {name = 'MAME EX'}}\n"
"function exports.startplugin()\n"
"  local n = 0\n"
"  local function go()\n"
"    local f = io.open(OUTPATH, 'w')\n"
"    if not f then return end\n"
"    local joy = manager.machine.input.device_classes.joystick\n"
"    if joy then\n"
"      for _, dev in ipairs(joy.devices) do\n"
"        f:write('D|' .. dev.devindex .. '|' .. dev.name .. '\\n')\n"
"        for id, item in pairs(dev.items) do\n"
"          f:write('I|' .. dev.devindex .. '|' .. item.name .. '|' .. item.token .. '|' .. tostring(item.current) .. '\\n')\n"
"        end\n"
"      end\n"
"    end\n"
"    f:write('END\\n')\n"
"    f:close()\n"
"  end\n"
"  exports.sub = emu.add_machine_frame_notifier(function() n = n + 1 if n == 20 then pcall(go) end end)\n"
"end\n"
"return exports\n";

static std::vector<std::string> split_bar(const std::string &s) {
    std::vector<std::string> v;
    size_t a = 0, b;
    while ((b = s.find('|', a)) != std::string::npos) { v.push_back(s.substr(a, b - a)); a = b + 1; }
    v.push_back(s.substr(a));
    return v;
}

static void parse_probe(const char *txt) {
    std::string t = txt;
    size_t pos = 0;
    g_pdev.clear();
    while (pos < t.size()) {
        size_t e = t.find('\n', pos);
        std::string line = t.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        pos = e == std::string::npos ? t.size() : e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> f = split_bar(line);
        if (f[0] == "D" && f.size() >= 3) { PDev d; d.index = atoi(f[1].c_str()); d.name = f[2]; g_pdev.push_back(d); }
        else if (f[0] == "I" && f.size() >= 5 && !g_pdev.empty()) {
            PItem it;
            it.name = f[2]; it.token = f[3]; it.cur = atoi(f[4].c_str());
            for (size_t i = 0; i < g_pdev.size(); i++) if (g_pdev[i].index == atoi(f[1].c_str())) g_pdev[i].items.push_back(it);
        }
    }
}

void padprobe_run(int force) {
    wchar_t exe[560], dir[560], sub[560], f[560], outp[560], fin[560], mdir[560], *cl;
    std::string lua, outu;
    char *u8;
    ArgList a = {0};
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    if (g_probed && !force) return;
    g_probed = 1;
    g_cacheTried = 1;
    if (!mame_exe(exe, 560)) return;
    mame_dir(mdir, 560);
    plugin_path(dir, 560, L"mame-ex-probe-plugin");
    path_join(sub, 560, dir, L"mamex_probe");
    CreateDirectoryW(dir, NULL);
    CreateDirectoryW(sub, NULL);
    path_join(fin, 560, g_root, L"mame-ex-padprobe.txt");   /* the result of an earlier run stays until a new one is complete */
    path_join(outp, 560, g_root, L"mame-ex-padprobe.new");
    DeleteFileW(outp);
    u8 = lua_path_u8(outp);
    outu = u8 ? u8 : "";
    free(u8);
    for (size_t q = 0; q < outu.size(); q++) if (outu[q] == '\\') outu[q] = '/';
    lua = "local OUTPATH = [=[" + outu + "]=]\n" + LUA;
    path_join(f, 560, sub, L"init.lua");
    if (!write_file(f, lua.c_str(), lua.size())) return;
    {
        const char *js = "{\n\t\"plugin\": {\n\t\t\"name\": \"mamex_probe\",\n\t\t\"description\": \"MAME EX pad probe\",\n\t\t\"version\": \"1.0.0\",\n\t\t\"author\": \"MAME EX\",\n\t\t\"type\": \"plugin\",\n\t\t\"start\": \"false\"\n\t}\n}\n";
        path_join(f, 560, sub, L"plugin.json");
        if (!write_file(f, js, strlen(js))) return;
    }
    mame_probe_args(&a, dir);
    cl = command_line_join(exe, &a);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(exe, cl, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, mdir, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 12000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    free(cl);
    args_free(&a);
    {
        char *txt = read_file(outp, NULL);
        if (txt) {
            if (strstr(txt, "END")) { parse_probe(txt); MoveFileExW(outp, fin, MOVEFILE_REPLACE_EXISTING); }
            free(txt);
        }
        DeleteFileW(outp);
    }
}

/* the MAME device number (1 = JOYCODE_1) of the n-th Xbox-type pad (XInput user n): the devices that have the items A, LB and LSX, in MAME's order */
int padprobe_devno(int user1) {
    int seen = 0;
    load_cache();
    for (size_t i = 0; i < g_pdev.size(); i++) {
        int a = 0, b = 0, c = 0;
        for (size_t k = 0; k < g_pdev[i].items.size(); k++) {
            const std::string &n = g_pdev[i].items[k].name;
            if (n == "A") a = 1; else if (n == "LB") b = 1; else if (n == "LSX") c = 1;
        }
        if (a && b && c && ++seen == user1) return g_pdev[i].index + 1;
    }
    return 0;
}

/* the token MAME uses for the item with this name on the device number devno (JOYCODE_devno); "" when unknown; cur: its value at rest */
int padprobe_token(int devno, const char *name, char *tok, size_t cap, int *cur) {
    load_cache();
    for (size_t i = 0; i < g_pdev.size(); i++) {
        if (g_pdev[i].index + 1 != devno) continue;
        for (size_t k = 0; k < g_pdev[i].items.size(); k++)
            if (g_pdev[i].items[k].name == name) {
                strncpy(tok, g_pdev[i].items[k].token.c_str(), cap - 1); tok[cap - 1] = 0;
                if (cur) *cur = g_pdev[i].items[k].cur;
                return 1;
            }
    }
    return 0;
}

/* the name MAME gives the item with this token on the device number devno ("A", "LT", "D-pad Up" ...) */
int padprobe_itemname(int devno, const char *token, char *name, size_t cap) {
    load_cache();
    for (size_t i = 0; i < g_pdev.size(); i++) {
        if (g_pdev[i].index + 1 != devno) continue;
        for (size_t k = 0; k < g_pdev[i].items.size(); k++)
            if (g_pdev[i].items[k].token == token) { strncpy(name, g_pdev[i].items[k].name.c_str(), cap - 1); name[cap - 1] = 0; return 1; }
    }
    return 0;
}

/* ---------------------------------------------------------------- the input log of a game run */
static const char *DIAG_LUA =
"local exports = {name = 'mamex_diag', version = '1.0.0', description = 'MAME EX input log', license = 'BSD-3-Clause', author = {name = 'MAME EX'}}\n"
"function exports.startplugin()\n"
"  local n, lines, fields = 0, 0, {}\n"
"  local function log(m)\n"
"    if lines > 700 then return end\n"
"    lines = lines + 1\n"
"    local f = io.open(OUTPATH, 'a')\n"
"    if f then f:write(m .. '\\n') f:close() end\n"
"  end\n"
"  local function setup()\n"
"    local input = manager.machine.input\n"
"    log('MAME ' .. tostring(emu.app_name and emu.app_name() or '?') .. ' ' .. tostring(emu.app_version and emu.app_version() or ''))\n"
"    for cname, cls in pairs(input.device_classes) do\n"
"      local c = 0\n"
"      for _, dev in ipairs(cls.devices) do\n"
"        c = c + 1\n"
"        log('device ' .. cname .. ' #' .. dev.devindex .. ' ' .. dev.name .. (cname == 'joystick' and (' (JOYCODE_' .. (dev.devindex + 1) .. ')') or ''))\n"
"        if cname == 'joystick' then\n"
"          for id, item in pairs(dev.items) do log('  item ' .. item.name .. ' = ' .. item.token) end\n"
"        end\n"
"      end\n"
"      log('class ' .. cname .. ': ' .. c .. ' devices, enabled=' .. tostring(cls.enabled))\n"
"    end\n"
"    for tag, port in pairs(manager.machine.ioport.ports) do\n"
"      for key, f in pairs(port.fields) do\n"
"        local nm = f.name or key\n"
"        if nm:match('^P%d ') or nm:match('Coin') or nm:match('Start') or nm:match('Service') then\n"
"          local ok, seq = pcall(function() return f:input_seq('standard') end)\n"
"          if ok then\n"
"            local tok = input:seq_to_tokens(seq)\n"
"            log('field ' .. tag .. ' | ' .. nm .. ' = ' .. tok)\n"
"            local parts = {}\n"
"            for p in (tok .. ' OR '):gmatch('(.-) OR ') do\n"
"              if p ~= '' then table.insert(parts, {tok = p, seq = input:seq_from_tokens(p), down = false}) end\n"
"            end\n"
"            table.insert(fields, {name = nm, parts = parts})\n"
"          end\n"
"        end\n"
"      end\n"
"    end\n"
"    log('--- press keys / pad buttons now: every change is logged ---')\n"
"  end\n"
"  local function frame()\n"
"    n = n + 1\n"
"    if n == 30 then setup() end\n"
"    if n <= 30 then return end\n"
"    local input = manager.machine.input\n"
"    for _, fl in ipairs(fields) do\n"
"      for _, p in ipairs(fl.parts) do\n"
"        local d = input:seq_pressed(p.seq)\n"
"        if d ~= p.down then p.down = d log('frame ' .. n .. ' ' .. fl.name .. ' via ' .. p.tok .. (d and ' PRESSED' or ' released')) end\n"
"      end\n"
"    end\n"
"  end\n"
"  exports.sub = emu.add_machine_frame_notifier(function() local ok, e = pcall(frame) if not ok then log('error: ' .. tostring(e)) end end)\n"
"end\n"
"return exports\n";

/* writes the log plugin (mame-ex-plugins\mame-ex-diag-plugin\mamex_diag); the log goes to mame-ex-input.log; 1 when done, dir = the folder for -pluginspath */
int diag_plugin_prepare(wchar_t *dirOut, size_t cap) {
    wchar_t dir[560], sub[560], f[560], outp[560];
    std::string lua, outu;
    char *u8;
    plugin_path(dir, 560, L"mame-ex-diag-plugin");
    path_join(sub, 560, dir, L"mamex_diag");
    CreateDirectoryW(dir, NULL);
    CreateDirectoryW(sub, NULL);
    log_path(outp, 560, L"mame-ex-input.log");
    DeleteFileW(outp);
    u8 = lua_path_u8(outp);
    outu = u8 ? u8 : "";
    free(u8);
    for (size_t q = 0; q < outu.size(); q++) if (outu[q] == '\\') outu[q] = '/';
    lua = "local OUTPATH = [=[" + outu + "]=]\n" + DIAG_LUA;
    path_join(f, 560, sub, L"init.lua");
    if (!write_file(f, lua.c_str(), lua.size())) return 0;
    {
        const char *js = "{\n\t\"plugin\": {\n\t\t\"name\": \"mamex_diag\",\n\t\t\"description\": \"MAME EX input log\",\n\t\t\"version\": \"1.0.0\",\n\t\t\"author\": \"MAME EX\",\n\t\t\"type\": \"plugin\",\n\t\t\"start\": \"false\"\n\t}\n}\n";
        path_join(f, 560, sub, L"plugin.json");
        if (!write_file(f, js, strlen(js))) return 0;
    }
    wcsncpy(dirOut, dir, cap - 1);
    dirOut[cap - 1] = 0;
    return 1;
}

/* 1 when the device number devno is an XInput pad, or nothing is known about it; 0 for a DirectInput / other device */
int padprobe_is_xinput(int devno) {
    load_cache();
    for (size_t i = 0; i < g_pdev.size(); i++)
        if (g_pdev[i].index + 1 == devno) return g_pdev[i].name.compare(0, 6, "XInput") == 0;
    return 1;
}
