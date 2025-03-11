#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

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
	? grow_da(slice, sizeof(*(slice)->data), arena), \
	  (slice)->data + (slice)->len++ \
	: (slice)->data + (slice)->len++)

#endif //ARENA_H_
