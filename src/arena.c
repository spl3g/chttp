#include <chttp/arena.h>

region *new_region(size_t cap) {
  size_t size_bytes = sizeof(region) + sizeof(uintptr_t)*cap;
  region *r = (region *)malloc(size_bytes);
  assert(r);
  r->next = NULL;
  r->len = 0;
  r->cap = cap;
  return r;
}

arena arena_init_cap(size_t cap) {
  arena a = {0};
  a.region_cap = cap;
  return a;
}

void *arena_alloc(arena *a, size_t size_bytes) {
  size_t size = (size_bytes + sizeof(uintptr_t)-1)/sizeof(uintptr_t);

  if (a->end == NULL) {
	assert(a->beg == NULL);
	size_t capacity = a->region_cap ? a->region_cap : ARENA_REGION_DEFAULT_CAPACITY;
	if (capacity < size) capacity = size;
	a->beg = new_region(capacity);
	a->end = a->beg;
  }

  while (a->end->len + size < a->end->cap && a->end->next != NULL) {
	a->end = a->end->next;
  }

  if (a->end->len + size > a->end->cap) {
	size_t capacity = a->region_cap ? a->region_cap : ARENA_REGION_DEFAULT_CAPACITY;
	if (capacity < size) capacity = size;
	a->beg = new_region(capacity);
	a->end->next = new_region(capacity);
	a->end = a->end->next;
  }

  void *p = &a->end->data[a->end->len];
  a->end->len += size;
  return memset(p, 0, size);
}

void arena_free(arena *a) {
  region *r = a->beg;
  while (r != NULL) {
	region *r0 = r;
	r = r0->next;
	free(r0);
  }
}

void *arena_realloc(arena *a, void *oldptr, size_t oldsz, size_t newsz) {
  if (newsz <= oldsz) return oldptr;
  void *newptr = arena_alloc(a, newsz);
  char *newptr_char = (char*)newptr;
  char *oldptr_char = (char*)oldptr;
  for (size_t i = 0; i < oldsz; ++i) {
    newptr_char[i] = oldptr_char[i];
  }
  return newptr;
}

void grow_da(void *slice, size_t size, arena *arena) {
  struct {
	void *data;
	size_t len;
	size_t cap;
  } replica;
  memcpy(&replica, slice, sizeof(replica));

  replica.cap = replica.cap ? replica.cap : 1;
  void *data = arena_alloc(arena, size*2*replica.cap);
  replica.cap *= 2;
  if (replica.len) {
	memcpy(data, replica.data, size*replica.len);
  }
  replica.data = data;

  memcpy(slice, &replica, sizeof(replica));
}
