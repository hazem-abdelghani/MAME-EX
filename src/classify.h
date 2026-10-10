#ifndef CLASSIFY_H
#define CLASSIFY_H
#include <wchar.h>
const wchar_t *classify_maker(const wchar_t *maker);   /* from MAME's manufacturer text ("Capcom / Arika" -> "Capcom"), or "Other" */
const wchar_t *classify_genre(const wchar_t *title);   /* "Fighting", "Shooter", "Racing", "Sports", "Puzzle", "Quiz", "Music" or "Other" */
const wchar_t *classify_region(const wchar_t *title);  /* from the words between ( ) in the title: "US", "JP", "ASIA", "WORLD", "EUROPE" or "Other" */
#endif
