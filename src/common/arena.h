#ifndef ARCANA_ARENA_H
#define ARCANA_ARENA_H

#include "arcana_common.h"

/*
 * ArcArena — simple bump allocator for compiler temporaries.
 *
 * All allocations come from contiguous blocks. Single arc_arena_free()
 * at the end of compilation deallocates everything.
 */

#define ARC_ARENA_BLOCK_SIZE (64 * 1024) /* 64 KB per block */

typedef struct ArcArenaBlock {
    struct ArcArenaBlock* next;
    size_t used;
    size_t cap;
    uint8_t data[];
} ArcArenaBlock;

typedef struct {
    ArcArenaBlock* head;       /* current block (most recent) */
    size_t         total;      /* total bytes allocated */
} ArcArena;

/* Initialize an empty arena */
void arc_arena_init(ArcArena* a);

/* Allocate size bytes from the arena (8-byte aligned) */
void* arc_arena_alloc(ArcArena* a, size_t size);

/* Free all memory in the arena */
void arc_arena_free(ArcArena* a);

/* Convenience: allocate a zero-initialized typed object */
#define ARC_ARENA_NEW(arena, type) \
    ((type*)arc_arena_alloc((arena), sizeof(type)))

/* Convenience: allocate a zero-initialized array */
#define ARC_ARENA_ARRAY(arena, type, count) \
    ((type*)arc_arena_alloc((arena), sizeof(type) * (count)))

#endif /* ARCANA_ARENA_H */
