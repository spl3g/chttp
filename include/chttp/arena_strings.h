#ifndef ARENA_STRINGS_H_
#define ARENA_STRINGS_H_

#include "arena.h"
#include "const_strings.h"

typedef struct {
  const_string *data;
  size_t len;
  size_t cap;
} const_string_da;

typedef struct {
  char* data;
  size_t len;
  size_t cap;
} string_builder;

const_string arena_cs_append(arena *a, const_string dst, const_string src);
const_string arena_cs_init(arena *a, int len);
const_string arena_cs_concat(arena *a, const_string_da strings, const_string sep);

#define arena_sb_append_buf(a, sb, buf, len) arena_da_append_many((a), (sb), (buf), (len))

#define arena_sb_append_cstr(a, sb, cstr)				\
    do {												\
        const char * str = (cstr);						\
        size_t len = strlen((cstr));					\
        arena_da_append_many((a), (sb), (str), len);	\
    } while (0)

#define arena_sb_append_null(a, sb)	arena_da_append_many((a), (sb), "", 1)

#define arena_sb_to_cs(sb) cs_from_parts((sb).data, (sb).len)

#endif // ARENA_STRINGS_H_
