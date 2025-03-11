#ifndef ARENA_STRINGS_H_
#define ARENA_STRINGS_H_

#include "arena.h"
#include "const_strings.h"

typedef struct {
  const_string *data;
  size_t len;
  size_t cap;
} const_string_da;

const_string arena_cs_append(arena *a, const_string dst, const_string src);
const_string arena_cs_init(arena *a, int len);
const_string arena_cs_concat(arena *a, const_string_da strings, const_string sep);

#endif // ARENA_STRINGS_H_
