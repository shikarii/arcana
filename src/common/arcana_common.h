#ifndef ARCANA_COMMON_H
#define ARCANA_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* --- Typed IDs --- */
typedef uint64_t ArcElementId;   /* stable source element identity */
typedef uint32_t ArcSymbolId;
typedef uint32_t ArcFunctionId;
typedef uint16_t ArcConstIdx;    /* index into constant pool */
typedef uint16_t ArcLocalIdx;    /* local variable slot */
typedef uint32_t ArcRegionId;
typedef uint32_t ArcNodeId;
typedef uint32_t ArcPortId;
typedef uint32_t ArcEdgeId;

#define ARC_INVALID_ID UINT32_MAX

/* --- Result / error helpers --- */
typedef enum {
    ARC_OK = 0,
    ARC_ERR_ALLOC,
    ARC_ERR_FORMAT,
    ARC_ERR_VERIFY,
    ARC_ERR_RUNTIME,
    ARC_ERR_COMPILE,
    ARC_ERR_IO,
    ARC_YIELD,           /* coroutine yielded (not an error) */
} ArcStatus;

/* --- Memory helpers --- */
#define ARC_ALLOC(type, count) ((type*)calloc((count), sizeof(type)))
#define ARC_REALLOC(ptr, type, count) ((type*)realloc((ptr), sizeof(type) * (count)))
#define ARC_FREE(ptr) free(ptr)

/* --- Dynamic byte buffer --- */
typedef struct {
    uint8_t* data;
    size_t   len;
    size_t   cap;
} ArcBuf;

static inline void arc_buf_init(ArcBuf* b) {
    b->data = NULL; b->len = 0; b->cap = 0;
}

static inline void arc_buf_free(ArcBuf* b) {
    ARC_FREE(b->data); b->data = NULL; b->len = b->cap = 0;
}

static inline void arc_buf_push(ArcBuf* b, uint8_t byte) {
    if (b->len >= b->cap) {
        b->cap = b->cap < 64 ? 64 : b->cap * 2;
        b->data = ARC_REALLOC(b->data, uint8_t, b->cap);
    }
    b->data[b->len++] = byte;
}

static inline void arc_buf_push_u16(ArcBuf* b, uint16_t v) {
    arc_buf_push(b, (uint8_t)(v & 0xFF));
    arc_buf_push(b, (uint8_t)(v >> 8));
}

static inline void arc_buf_push_i32(ArcBuf* b, int32_t v) {
    uint32_t u = (uint32_t)v;
    arc_buf_push(b, (uint8_t)(u & 0xFF));
    arc_buf_push(b, (uint8_t)((u >> 8) & 0xFF));
    arc_buf_push(b, (uint8_t)((u >> 16) & 0xFF));
    arc_buf_push(b, (uint8_t)((u >> 24) & 0xFF));
}

static inline uint16_t arc_read_u16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline int32_t arc_read_i32(const uint8_t* p) {
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return (int32_t)u;
}

#endif /* ARCANA_COMMON_H */
