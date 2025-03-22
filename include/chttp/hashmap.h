#ifndef HASHMAP_H_
#define HASHMAP_H_

#include <stddef.h>
#include "const_strings.h"
#include "arena.h"

#define HASHMAP_DEFAULT_R 31

typedef struct hashitem hashitem;
struct hashitem {
  hashitem *next;
  const_string key;
  void* val;
};

typedef struct {
  size_t cap;
  hashitem data[];
} hashmap;

size_t hash_str(const_string str, size_t cap);
hashmap *hashmap_init(arena *a, size_t cap);
size_t hashmap_kstr_insert(arena *a, hashmap* map, const_string key, void* val);
void *hashmap_kstr_get(hashmap *map, const_string key);

#endif // HASHMAP_H_
