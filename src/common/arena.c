#include "arena.h"

void arc_arena_init(ArcArena* a) {
    a->head = NULL;
    a->total = 0;
}

static ArcArenaBlock* arena_new_block(size_t min_size) {
    size_t cap = min_size > ARC_ARENA_BLOCK_SIZE ? min_size : ARC_ARENA_BLOCK_SIZE;
    ArcArenaBlock* blk = (ArcArenaBlock*)calloc(1, sizeof(ArcArenaBlock) + cap);
    if (!blk) return NULL;
    blk->next = NULL;
    blk->used = 0;
    blk->cap = cap;
    return blk;
}

void* arc_arena_alloc(ArcArena* a, size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    /* Check if current block has room */
    if (a->head && a->head->used + size <= a->head->cap) {
        void* ptr = a->head->data + a->head->used;
        a->head->used += size;
        a->total += size;
        memset(ptr, 0, size);
        return ptr;
    }

    /* Allocate a new block */
    ArcArenaBlock* blk = arena_new_block(size);
    if (!blk) return NULL;
    blk->next = a->head;
    a->head = blk;
    void* ptr = blk->data;
    blk->used = size;
    a->total += size;
    memset(ptr, 0, size);
    return ptr;
}

void arc_arena_free(ArcArena* a) {
    ArcArenaBlock* blk = a->head;
    while (blk) {
        ArcArenaBlock* next = blk->next;
        free(blk);
        blk = next;
    }
    a->head = NULL;
    a->total = 0;
}
