#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

typedef struct region region;

struct region {
  region *next;
  size_t cap;
  size_t len;
  uintptr_t data[];
};

typedef struct {
  region *beg;
  region *end;
} arena;

void *arena_alloc(arena *a, size_t size);
void arena_free(arena *a);
void grow_da(void *slice, size_t size, arena *arena);


#define new(a, t, c) (t *)arena_alloc(a, sizeof(t)*c)

#define ARENA_REGION_DEFAULT_CAPACITY (8*1024);

// from nullprogram
#define push(slice, arena) \
  ((slice)->len >= (slice)->cap \
	? grow(slice, sizeof(*(slice)->data), arena), \
	  (slice)->data + (slice)->len++ \
	: (slice)->data + (slice)->len++)

#endif //ARENA_H_

#ifdef ARENA_IMPLEMENTATION

region *new_region(size_t cap) {
  size_t size_bytes = sizeof(region) + sizeof(uintptr_t)*cap;
  region *r = (region *)malloc(size_bytes);
  assert(r);
  r->next = NULL;
  r->len = 0;
  r->cap = cap;
  return r;
}

void *arena_alloc(arena *a, size_t size_bytes) {
  size_t size = (size_bytes + sizeof(uintptr_t)-1)/sizeof(uintptr_t);
  
  if (a->end == NULL) {
	assert(a->beg == NULL);
	size_t capacity = ARENA_REGION_DEFAULT_CAPACITY;
	if (capacity < size) capacity = size;
	a->beg = new_region(capacity);
	a->end = a->beg;
  }

  while (a->end->len + size < a->end->cap && a->end->next != NULL) {
	a->end = a->end->next;
  }
  
  if (a->end->len + size > a->end->cap) {
	size_t capacity = ARENA_REGION_DEFAULT_CAPACITY;
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

void grow(void *slice, size_t size, arena *arena) {
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
#endif // ARENA_IMPLEMENTATION
