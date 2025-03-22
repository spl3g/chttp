#include <chttp/hashmap.h>

size_t hash_str(const_string str, size_t cap) {
  size_t hash = 0;
  for (int i = 0; i < str.len; i++) {
	hash = (HASHMAP_DEFAULT_R * hash + str.data[i]) % cap;
  }
  return hash;
}

hashmap *hashmap_init(arena *a, size_t cap) {
  hashmap *map = arena_alloc(a, sizeof(hashmap) + sizeof(ptrdiff_t) * cap);
  map->cap = cap;
  return map;
}

size_t hashmap_kstr_insert(arena *a, hashmap* map, const_string key, void* val) {
  size_t hash = hash_str(key, map->cap);
  if (map->data[hash].key.len > 0) {
	map->data[hash].next = arena_alloc(a, sizeof(hashitem));
	map->data[hash].next->key = key;
	map->data[hash].next->val = val;
  } else {
	map->data[hash].key = key;
	map->data[hash].val = val;
  }

  return hash;
}

void *hashmap_kstr_get(hashmap *map, const_string key) {
  size_t hash = hash_str(key, map->cap);
  hashitem *item = &map->data[hash];
  while (item->next != NULL && !cs_eq(item->key, key)) {
	item = item->next;
  }

  if (!cs_eq(item->key, key)) {
	return NULL;
  }

  return item->val;
}
