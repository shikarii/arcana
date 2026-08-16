#include "format.h"
#include "opcodes.h"

/* === Constant Pool === */

void arc_const_pool_init(ArcConstPool* p) {
    p->entries = NULL; p->count = 0; p->cap = 0;
}

void arc_const_pool_free(ArcConstPool* p) {
    for (uint16_t i = 0; i < p->count; i++) {
        if (p->entries[i].tag == ARC_CONST_STRING)
            ARC_FREE(p->entries[i].as.str.data);
    }
    ARC_FREE(p->entries);
    p->entries = NULL; p->count = p->cap = 0;
}

static uint16_t pool_grow(ArcConstPool* p) {
    if (p->count >= p->cap) {
        p->cap = p->cap < 16 ? 16 : p->cap * 2;
        p->entries = ARC_REALLOC(p->entries, ArcConstant, p->cap);
    }
    return p->count++;
}

uint16_t arc_const_pool_add_null(ArcConstPool* p) {
    for (uint16_t i = 0; i < p->count; i++)
        if (p->entries[i].tag == ARC_CONST_NULL) return i;
    uint16_t idx = pool_grow(p);
    p->entries[idx].tag = ARC_CONST_NULL;
    return idx;
}

uint16_t arc_const_pool_add_bool(ArcConstPool* p, bool v) {
    for (uint16_t i = 0; i < p->count; i++)
        if (p->entries[i].tag == ARC_CONST_BOOL && p->entries[i].as.b == v) return i;
    uint16_t idx = pool_grow(p);
    p->entries[idx].tag = ARC_CONST_BOOL;
    p->entries[idx].as.b = v;
    return idx;
}

uint16_t arc_const_pool_add_i64(ArcConstPool* p, int64_t v) {
    for (uint16_t i = 0; i < p->count; i++)
        if (p->entries[i].tag == ARC_CONST_I64 && p->entries[i].as.i64 == v) return i;
    uint16_t idx = pool_grow(p);
    p->entries[idx].tag = ARC_CONST_I64;
    p->entries[idx].as.i64 = v;
    return idx;
}

uint16_t arc_const_pool_add_f64(ArcConstPool* p, double v) {
    for (uint16_t i = 0; i < p->count; i++)
        if (p->entries[i].tag == ARC_CONST_F64 && p->entries[i].as.f64 == v) return i;
    uint16_t idx = pool_grow(p);
    p->entries[idx].tag = ARC_CONST_F64;
    p->entries[idx].as.f64 = v;
    return idx;
}

uint16_t arc_const_pool_add_string(ArcConstPool* p, const char* s, uint32_t len) {
    for (uint16_t i = 0; i < p->count; i++)
        if (p->entries[i].tag == ARC_CONST_STRING &&
            p->entries[i].as.str.len == len &&
            memcmp(p->entries[i].as.str.data, s, len) == 0) return i;
    uint16_t idx = pool_grow(p);
    p->entries[idx].tag = ARC_CONST_STRING;
    p->entries[idx].as.str.data = ARC_ALLOC(char, len + 1);
    memcpy(p->entries[idx].as.str.data, s, len);
    p->entries[idx].as.str.data[len] = '\0';
    p->entries[idx].as.str.len = len;
    return idx;
}

/* === Function Table === */

void arc_func_table_init(ArcFuncTable* t) {
    t->funcs = NULL; t->count = 0; t->cap = 0;
}

void arc_func_table_free(ArcFuncTable* t) {
    ARC_FREE(t->funcs); t->funcs = NULL; t->count = t->cap = 0;
}

uint16_t arc_func_table_add(ArcFuncTable* t, ArcFuncRecord rec) {
    if (t->count >= t->cap) {
        t->cap = t->cap < 8 ? 8 : t->cap * 2;
        t->funcs = ARC_REALLOC(t->funcs, ArcFuncRecord, t->cap);
    }
    t->funcs[t->count] = rec;
    return t->count++;
}

/* === Debug Table === */

void arc_debug_table_init(ArcDebugTable* t) {
    t->entries = NULL; t->count = 0; t->cap = 0;
}

void arc_debug_table_free(ArcDebugTable* t) {
    ARC_FREE(t->entries); t->entries = NULL; t->count = t->cap = 0;
}

void arc_debug_table_add(ArcDebugTable* t, ArcDebugEntry e) {
    if (t->count >= t->cap) {
        t->cap = t->cap < 16 ? 16 : t->cap * 2;
        t->entries = ARC_REALLOC(t->entries, ArcDebugEntry, t->cap);
    }
    t->entries[t->count++] = e;
}

/* === Bytecode Image === */

void arc_image_init(ArcBytecodeImage* img) {
    arc_const_pool_init(&img->constants);
    arc_func_table_init(&img->functions);
    img->code = NULL; img->code_len = 0;
    arc_debug_table_init(&img->debug);
}

void arc_image_free(ArcBytecodeImage* img) {
    arc_const_pool_free(&img->constants);
    arc_func_table_free(&img->functions);
    ARC_FREE(img->code); img->code = NULL; img->code_len = 0;
    arc_debug_table_free(&img->debug);
}

/* === .mgc Binary Serialization === */

static void write_u8(ArcBuf* b, uint8_t v) { arc_buf_push(b, v); }
static void write_u16(ArcBuf* b, uint16_t v) { arc_buf_push_u16(b, v); }
static void write_u32(ArcBuf* b, uint32_t v) { arc_buf_push_i32(b, (int32_t)v); }
static void write_i64(ArcBuf* b, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) { arc_buf_push(b, (uint8_t)(u & 0xFF)); u >>= 8; }
}
static void write_f64(ArcBuf* b, double v) {
    uint64_t u; memcpy(&u, &v, 8); write_i64(b, (int64_t)u);
}
static void write_bytes(ArcBuf* b, const void* p, size_t n) {
    for (size_t i = 0; i < n; i++) arc_buf_push(b, ((const uint8_t*)p)[i]);
}

ArcStatus arc_image_write(const ArcBytecodeImage* img, uint8_t** out, size_t* out_len) {
    ArcBuf b; arc_buf_init(&b);

    /* Header: magic(4) + version(2) + flags(2) + const_count(2) + func_count(2) */
    write_u8(&b, ARC_MAGIC_0); write_u8(&b, ARC_MAGIC_1);
    write_u8(&b, ARC_MAGIC_2); write_u8(&b, ARC_MAGIC_3);
    write_u8(&b, ARC_VERSION_MAJOR); write_u8(&b, ARC_VERSION_MINOR);
    write_u16(&b, 0); /* flags */
    write_u16(&b, img->constants.count);
    write_u16(&b, img->functions.count);

    /* Constant pool */
    for (uint16_t i = 0; i < img->constants.count; i++) {
        const ArcConstant* c = &img->constants.entries[i];
        write_u8(&b, (uint8_t)c->tag);
        switch (c->tag) {
        case ARC_CONST_NULL: break;
        case ARC_CONST_BOOL: write_u8(&b, c->as.b ? 1 : 0); break;
        case ARC_CONST_I64: write_i64(&b, c->as.i64); break;
        case ARC_CONST_F64: write_f64(&b, c->as.f64); break;
        case ARC_CONST_STRING:
            write_u32(&b, c->as.str.len);
            write_bytes(&b, c->as.str.data, c->as.str.len);
            break;
        }
    }

    /* Function table */
    for (uint16_t i = 0; i < img->functions.count; i++) {
        const ArcFuncRecord* f = &img->functions.funcs[i];
        write_u16(&b, f->name_const_idx);
        write_u8(&b, f->arity);
        write_u16(&b, f->local_count);
        write_u16(&b, f->max_stack);
        write_u32(&b, f->code_offset);
        write_u32(&b, f->code_length);
    }

    /* Code section: u32 length + bytes */
    write_u32(&b, img->code_len);
    write_bytes(&b, img->code, img->code_len);

    /* Debug section: u32 entry_count + entries */
    write_u32(&b, img->debug.count);
    for (uint32_t i = 0; i < img->debug.count; i++) {
        const ArcDebugEntry* d = &img->debug.entries[i];
        write_u16(&b, d->func_idx);
        write_u32(&b, d->bc_start);
        write_u32(&b, d->bc_end);
        write_i64(&b, (int64_t)d->element_id);
    }

    *out = b.data; *out_len = b.len;
    return ARC_OK;
}

/* === .mgc Binary Deserialization === */

typedef struct { const uint8_t* p; const uint8_t* end; } Reader;

static bool read_u8(Reader* r, uint8_t* v) {
    if (r->p >= r->end) return false;
    *v = *r->p++; return true;
}
static bool read_u16(Reader* r, uint16_t* v) {
    if (r->p + 2 > r->end) return false;
    *v = arc_read_u16(r->p); r->p += 2; return true;
}
static bool read_u32(Reader* r, uint32_t* v) {
    if (r->p + 4 > r->end) return false;
    *v = (uint32_t)arc_read_i32(r->p); r->p += 4; return true;
}
static bool read_i64(Reader* r, int64_t* v) {
    if (r->p + 8 > r->end) return false;
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) u |= ((uint64_t)r->p[i]) << (i * 8);
    *v = (int64_t)u; r->p += 8; return true;
}
static bool read_f64(Reader* r, double* v) {
    int64_t i; if (!read_i64(r, &i)) return false;
    memcpy(v, &i, 8); return true;
}

ArcStatus arc_image_read(const uint8_t* data, size_t len, ArcBytecodeImage* img) {
    arc_image_init(img);
    Reader r = { data, data + len };
    uint8_t m0, m1, m2, m3, vmaj, vmin;
    uint16_t flags, const_count, func_count;

    if (!read_u8(&r, &m0) || !read_u8(&r, &m1) || !read_u8(&r, &m2) || !read_u8(&r, &m3))
        return ARC_ERR_FORMAT;
    if (m0 != ARC_MAGIC_0 || m1 != ARC_MAGIC_1 || m2 != ARC_MAGIC_2 || m3 != ARC_MAGIC_3)
        return ARC_ERR_FORMAT;

    if (!read_u8(&r, &vmaj) || !read_u8(&r, &vmin)) return ARC_ERR_FORMAT;
    if (!read_u16(&r, &flags)) return ARC_ERR_FORMAT;
    if (!read_u16(&r, &const_count) || !read_u16(&r, &func_count)) return ARC_ERR_FORMAT;

    /* Constants */
    for (uint16_t i = 0; i < const_count; i++) {
        uint8_t tag;
        if (!read_u8(&r, &tag)) return ARC_ERR_FORMAT;
        switch (tag) {
        case ARC_CONST_NULL: arc_const_pool_add_null(&img->constants); break;
        case ARC_CONST_BOOL: {
            uint8_t v; if (!read_u8(&r, &v)) return ARC_ERR_FORMAT;
            arc_const_pool_add_bool(&img->constants, v != 0); break;
        }
        case ARC_CONST_I64: {
            int64_t v; if (!read_i64(&r, &v)) return ARC_ERR_FORMAT;
            arc_const_pool_add_i64(&img->constants, v); break;
        }
        case ARC_CONST_F64: {
            double v; if (!read_f64(&r, &v)) return ARC_ERR_FORMAT;
            arc_const_pool_add_f64(&img->constants, v); break;
        }
        case ARC_CONST_STRING: {
            uint32_t slen; if (!read_u32(&r, &slen)) return ARC_ERR_FORMAT;
            if (r.p + slen > r.end) return ARC_ERR_FORMAT;
            arc_const_pool_add_string(&img->constants, (const char*)r.p, slen);
            r.p += slen; break;
        }
        default: return ARC_ERR_FORMAT;
        }
    }

    /* Functions */
    for (uint16_t i = 0; i < func_count; i++) {
        ArcFuncRecord fr;
        if (!read_u16(&r, &fr.name_const_idx)) return ARC_ERR_FORMAT;
        if (!read_u8(&r, &fr.arity)) return ARC_ERR_FORMAT;
        if (!read_u16(&r, &fr.local_count)) return ARC_ERR_FORMAT;
        if (!read_u16(&r, &fr.max_stack)) return ARC_ERR_FORMAT;
        if (!read_u32(&r, &fr.code_offset)) return ARC_ERR_FORMAT;
        if (!read_u32(&r, &fr.code_length)) return ARC_ERR_FORMAT;
        arc_func_table_add(&img->functions, fr);
    }

    /* Code */
    uint32_t clen;
    if (!read_u32(&r, &clen)) return ARC_ERR_FORMAT;
    if (r.p + clen > r.end) return ARC_ERR_FORMAT;
    img->code = ARC_ALLOC(uint8_t, clen);
    memcpy(img->code, r.p, clen);
    img->code_len = clen;
    r.p += clen;

    /* Debug (optional — if data remains) */
    uint32_t dcount = 0;
    if (r.p < r.end) {
        if (!read_u32(&r, &dcount)) return ARC_ERR_FORMAT;
        for (uint32_t i = 0; i < dcount; i++) {
            ArcDebugEntry de;
            if (!read_u16(&r, &de.func_idx)) return ARC_ERR_FORMAT;
            if (!read_u32(&r, &de.bc_start)) return ARC_ERR_FORMAT;
            if (!read_u32(&r, &de.bc_end)) return ARC_ERR_FORMAT;
            int64_t eid; if (!read_i64(&r, &eid)) return ARC_ERR_FORMAT;
            de.element_id = (ArcElementId)eid;
            arc_debug_table_add(&img->debug, de);
        }
    }

    return ARC_OK;
}
