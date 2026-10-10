/* Controls profiles: the key / pad mapping and the controller options under a name (profiles\\<name>.cfg next to the launcher). */
#include "common.h"
#include <vector>
#include <string>
#include <algorithm>

static void dir_path(wchar_t *out, size_t cap) { path_join(out, cap, g_root, L"profiles"); }
static void file_path(const wchar_t *name, wchar_t *out, size_t cap) {
    wchar_t dir[560], f[100];
    dir_path(dir, 560);
    _snwprintf(f, 100, L"%ls.cfg", name);
    f[99] = 0;
    path_join(out, cap, dir, f);
}

void profile_clean_name(wchar_t *name) {
    wchar_t *r, *w = name;
    for (r = name; *r; r++) if (!wcschr(L"\\/:*?\"<>|", *r) && *r >= 32) *w++ = *r;
    *w = 0;
    trim_w(name);
    if (wcslen(name) > 60) name[60] = 0;
    trim_w(name);
    { size_t l = wcslen(name); while (l && name[l - 1] == L'.') name[--l] = 0; }   /* a name that ends with a dot is not a file name */
}

int profile_exists(const wchar_t *name) { wchar_t p[700]; file_path(name, p, 700); return file_exists(p); }

int profile_names(wchar_t names[][64], int max) {
    wchar_t pat[700], dir[560];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    std::vector<std::wstring> v;
    int n = 0;
    dir_path(dir, 560);
    path_join(pat, 700, dir, L"*.cfg");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        size_t l = wcslen(fd.cFileName);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || l < 5 || l > 64) continue;
        v.push_back(std::wstring(fd.cFileName, l - 4));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(v.begin(), v.end(), [](const std::wstring &a, const std::wstring &b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    for (const auto &s : v) { if (n >= max) break; wcsncpy(names[n], s.c_str(), 63); names[n][63] = 0; n++; }
    return n;
}

char *profile_load(const wchar_t *name) { wchar_t p[700]; file_path(name, p, 700); return read_file(p, NULL); }

int profile_save(const wchar_t *name, const char *text) {
    wchar_t p[700], dir[560];
    dir_path(dir, 560);
    CreateDirectoryW(dir, NULL);
    file_path(name, p, 700);
    return write_file(p, text, strlen(text));
}

int profile_rename(const wchar_t *from, const wchar_t *to) {
    wchar_t a[700], b[700];
    file_path(from, a, 700);
    file_path(to, b, 700);
    if (!_wcsicmp(from, to)) {   /* only the case of the letters: through a temporary name */
        wchar_t t[700];
        _snwprintf(t, 700, L"%ls.tmp", a);
        return MoveFileW(a, t) && MoveFileW(t, b);
    }
    return MoveFileW(a, b);   /* fails when the new name exists */
}

int profile_delete(const wchar_t *name) { wchar_t p[700]; file_path(name, p, 700); return DeleteFileW(p) != 0; }
