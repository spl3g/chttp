// Most of the code is from https://github.com/tsoding/arena

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
  size_t region_cap;
} arena;

arena arena_init_cap(size_t cap);
void *arena_alloc(arena *a, size_t size);
void arena_free(arena *a);
void *arena_realloc(arena *a, void *oldptr, size_t oldsz, size_t newsz);

#define new(a, t, c) (t *)arena_alloc(a, sizeof(t)*c)

#define ARENA_REGION_DEFAULT_CAPACITY (8*1024)
#define ARENA_DA_INIT_CAP (2)

#define arena_da_reserve(a, da, expected_size)								\
    do {																	\
        if ((expected_size) >= (da)->cap) {									\
        size_t new_cap = (da)->cap == 0 ? ARENA_DA_INIT_CAP : (da)->cap*2;	\
        while (new_cap < expected_size) {									\
            new_cap *= 2;													\
        }																	\
        (da)->data = arena_realloc(											\
             (a), (da)->data,												\
             (da)->cap*sizeof(*(da)->data),									\
             new_cap*sizeof(*(da)->data));									\
        (da)->cap = new_cap;												\
        }																	\
    } while (0)

#define arena_da_append(a, da, item)				\
    do {											\
        arena_da_reserve((a), (da), (da)->len + 1);	\
        (da)->data[(da)->len++] = (item);			\
    } while (0)

#define arena_da_append_many(a, da, items, items_len)							\
    do {																		\
        arena_da_reserve((a), (da), (da)->len+len);								\
        memcpy((da)->data+(da)->len, (items), sizeof(*(da)->data)*items_len);	\
        (da)->len += len;														\
    } while (0)

#endif //ARENA_H_
