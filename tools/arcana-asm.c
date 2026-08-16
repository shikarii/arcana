/*
 * arcana-asm — Assemble text assembly to .mgc bytecode.
 *
 * Usage: arcana-asm <input.asm> <output.mgc>
 *
 * Assembly format:
 *   .const #N <type> <value>     ; declare constant
 *   .func <name> arity=N locals=N max_stack=N
 *     <mnemonic> [operands]
 *   .end
 *
 * Example:
 *   .const #0 string "main"
 *   .const #1 i64 5
 *   .const #2 i64 10
 *   .func main arity=0 locals=0 max_stack=2
 *     const #1
 *     const #2
 *     add
 *     halt
 *   .end
 */

#include "../src/common/arcana_common.h"
#include "../src/bytecode/format.h"
#include "../src/bytecode/opcodes.h"
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_LABELS 256

typedef struct {
    char name[64];
    uint32_t offset;
} Label;

typedef struct {
    char label[64];
    uint32_t patch_pos;
    uint32_t instr_end; /* ip after operand bytes, for relative offset calc */
} Fixup;

typedef struct {
    ArcBytecodeImage image;
    ArcBuf code;
    Label labels[MAX_LABELS];
    int label_count;
    Fixup fixups[MAX_LABELS];
    int fixup_count;
    int line_num;
    bool in_func;
    uint32_t func_code_start;
} Assembler;

static char* skip_ws(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static char* read_token(char* s, char* out, int max) {
    s = skip_ws(s);
    int i = 0;
    while (*s && !isspace((unsigned char)*s) && *s != ';' && i < max - 1)
        out[i++] = *s++;
    out[i] = '\0';
    return s;
}

static int parse_int(const char* s, int64_t* out) {
    char* end;
    *out = strtoll(s, &end, 10);
    return *end == '\0' || isspace((unsigned char)*end);
}

static uint8_t lookup_opcode(const char* mnemonic) {
#define ARC_OP_LOOKUP(name, byte, operands, pops, pushes, mn) \
    if (strcmp(mnemonic, mn) == 0) return byte;
    ARC_OPCODES(ARC_OP_LOOKUP)
#undef ARC_OP_LOOKUP
    return 0xFF; /* will match HALT if unknown, but we check separately */
}

static bool is_opcode(const char* mnemonic) {
#define ARC_OP_CHECK(name, byte, operands, pops, pushes, mn) \
    if (strcmp(mnemonic, mn) == 0) return true;
    ARC_OPCODES(ARC_OP_CHECK)
#undef ARC_OP_CHECK
    return false;
}

static bool assemble_line(Assembler* a, char* line) {
    char* p = skip_ws(line);

    /* Skip empty lines and comments */
    if (*p == '\0' || *p == ';' || *p == '#') return true;

    /* Labels: "label:" */
    char* colon = strchr(p, ':');
    if (colon && colon > p && (colon[1] == '\0' || isspace((unsigned char)colon[1]))) {
        /* Check it's not a directive */
        if (*p != '.') {
            int len = (int)(colon - p);
            if (len > 0 && len < 64 && a->label_count < MAX_LABELS) {
                memcpy(a->labels[a->label_count].name, p, (size_t)len);
                a->labels[a->label_count].name[len] = '\0';
                a->labels[a->label_count].offset = (uint32_t)a->code.len;
                a->label_count++;
            }
            return true;
        }
    }

    char tok[128];
    p = read_token(p, tok, sizeof(tok));

    /* Directives */
    if (strcmp(tok, ".const") == 0) {
        char idx_str[16], type[16];
        p = read_token(p, idx_str, sizeof(idx_str)); /* #N */
        p = read_token(p, type, sizeof(type));

        if (strcmp(type, "null") == 0) {
            arc_const_pool_add_null(&a->image.constants);
        } else if (strcmp(type, "bool") == 0) {
            char val[16];
            read_token(p, val, sizeof(val));
            arc_const_pool_add_bool(&a->image.constants, strcmp(val, "true") == 0);
        } else if (strcmp(type, "i64") == 0) {
            char val[32];
            read_token(p, val, sizeof(val));
            int64_t v;
            if (parse_int(val, &v)) arc_const_pool_add_i64(&a->image.constants, v);
        } else if (strcmp(type, "f64") == 0) {
            char val[32];
            read_token(p, val, sizeof(val));
            double v = strtod(val, NULL);
            arc_const_pool_add_f64(&a->image.constants, v);
        } else if (strcmp(type, "string") == 0) {
            p = skip_ws(p);
            if (*p == '"') {
                p++;
                char str[256];
                int si = 0;
                while (*p && *p != '"' && si < 255) str[si++] = *p++;
                str[si] = '\0';
                arc_const_pool_add_string(&a->image.constants, str, (uint32_t)si);
            }
        }
        return true;
    }

    if (strcmp(tok, ".func") == 0) {
        char name[64];
        p = read_token(p, name, sizeof(name));

        uint8_t arity = 0;
        uint16_t locals = 0, max_stack = 0;

        /* Parse key=value pairs */
        while (*skip_ws(p)) {
            char kv[64];
            p = read_token(p, kv, sizeof(kv));
            if (sscanf(kv, "arity=%hhu", &arity) == 1) continue;
            int tmp;
            if (sscanf(kv, "locals=%d", &tmp) == 1) { locals = (uint16_t)tmp; continue; }
            if (sscanf(kv, "max_stack=%d", &tmp) == 1) { max_stack = (uint16_t)tmp; continue; }
        }

        uint16_t name_ci = arc_const_pool_add_string(&a->image.constants,
                                                      name, (uint32_t)strlen(name));
        a->func_code_start = (uint32_t)a->code.len;
        a->in_func = true;

        ArcFuncRecord rec = {
            .name_const_idx = name_ci,
            .arity = arity,
            .local_count = locals,
            .max_stack = max_stack,
            .code_offset = a->func_code_start,
            .code_length = 0 /* filled in at .end */
        };
        arc_func_table_add(&a->image.functions, rec);
        return true;
    }

    if (strcmp(tok, ".end") == 0) {
        if (a->in_func && a->image.functions.count > 0) {
            uint16_t fi = a->image.functions.count - 1;
            a->image.functions.funcs[fi].code_length =
                (uint32_t)a->code.len - a->func_code_start;
        }
        a->in_func = false;
        return true;
    }

    /* Instructions */
    if (!is_opcode(tok)) {
        fprintf(stderr, "line %d: unknown instruction '%s'\n", a->line_num, tok);
        return false;
    }

    uint8_t op = lookup_opcode(tok);
    arc_buf_push(&a->code, op);
    int ob = arc_op_operand_bytes(op);

    switch (op) {
    case OP_CONST:
    case OP_LOAD_LOCAL:
    case OP_STORE_LOCAL:
    case OP_LOAD_GLOBAL:
    case OP_STORE_GLOBAL: {
        char operand[32];
        p = read_token(p, operand, sizeof(operand));
        char* start = operand;
        if (*start == '#') start++;
        int64_t v;
        if (!parse_int(start, &v)) {
            fprintf(stderr, "line %d: bad operand '%s'\n", a->line_num, operand);
            return false;
        }
        arc_buf_push_u16(&a->code, (uint16_t)v);
        break;
    }
    case OP_JUMP:
    case OP_JUMP_IF_FALSE:
    case OP_JUMP_IF_TRUE: {
        char operand[64];
        p = read_token(p, operand, sizeof(operand));
        /* Check if it's a label reference or a raw offset */
        int64_t v;
        if (parse_int(operand, &v)) {
            arc_buf_push_i32(&a->code, (int32_t)v);
        } else {
            /* Label fixup */
            if (a->fixup_count < MAX_LABELS) {
                strncpy(a->fixups[a->fixup_count].label, operand, 63);
                a->fixups[a->fixup_count].label[63] = '\0';
                a->fixups[a->fixup_count].patch_pos = (uint32_t)a->code.len;
                a->fixups[a->fixup_count].instr_end = (uint32_t)(a->code.len + 4);
                a->fixup_count++;
            }
            arc_buf_push_i32(&a->code, 0); /* placeholder */
        }
        break;
    }
    case OP_CALL: {
        char fi_str[16], argc_str[16];
        p = read_token(p, fi_str, sizeof(fi_str));
        p = read_token(p, argc_str, sizeof(argc_str));
        int64_t fi, argc_v;
        /* Accept func=N argc=N or just N N */
        char* s1 = fi_str; if (strncmp(s1, "func=", 5) == 0) s1 += 5;
        char* s2 = argc_str; if (strncmp(s2, "argc=", 5) == 0) s2 += 5;
        parse_int(s1, &fi);
        parse_int(s2, &argc_v);
        arc_buf_push_u16(&a->code, (uint16_t)fi);
        arc_buf_push(&a->code, (uint8_t)argc_v);
        arc_buf_push(&a->code, 0); /* padding */
        break;
    }
    case OP_INTRINSIC: {
        char id_str[16], argc_str[16];
        p = read_token(p, id_str, sizeof(id_str));
        p = read_token(p, argc_str, sizeof(argc_str));
        int64_t id, argc_v;
        /* Accept name or number */
        if (strcmp(id_str, "print") == 0) id = ARC_INTRINSIC_PRINT;
        else if (strcmp(id_str, "clock") == 0) id = ARC_INTRINSIC_CLOCK;
        else parse_int(id_str, &id);
        char* s2 = argc_str; if (strncmp(s2, "argc=", 5) == 0) s2 += 5;
        parse_int(s2, &argc_v);
        arc_buf_push_u16(&a->code, (uint16_t)id);
        arc_buf_push(&a->code, (uint8_t)argc_v);
        arc_buf_push(&a->code, 0); /* padding */
        break;
    }
    default:
        /* Zero-operand instructions — nothing to emit */
        (void)ob;
        break;
    }

    return true;
}

static bool resolve_fixups(Assembler* a) {
    for (int i = 0; i < a->fixup_count; i++) {
        Fixup* f = &a->fixups[i];
        /* Find label */
        uint32_t target = UINT32_MAX;
        for (int j = 0; j < a->label_count; j++) {
            if (strcmp(a->labels[j].name, f->label) == 0) {
                target = a->labels[j].offset;
                break;
            }
        }
        if (target == UINT32_MAX) {
            fprintf(stderr, "error: undefined label '%s'\n", f->label);
            return false;
        }
        int32_t offset = (int32_t)target - (int32_t)f->instr_end;
        a->code.data[f->patch_pos]     = (uint8_t)(((uint32_t)offset) & 0xFF);
        a->code.data[f->patch_pos + 1] = (uint8_t)((((uint32_t)offset) >> 8) & 0xFF);
        a->code.data[f->patch_pos + 2] = (uint8_t)((((uint32_t)offset) >> 16) & 0xFF);
        a->code.data[f->patch_pos + 3] = (uint8_t)((((uint32_t)offset) >> 24) & 0xFF);
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: arcana-asm <input.asm> <output.mgc>\n");
        return 1;
    }

    FILE* in = fopen(argv[1], "r");
    if (!in) {
        fprintf(stderr, "error: cannot read '%s'\n", argv[1]);
        return 1;
    }

    Assembler a = {0};
    arc_image_init(&a.image);
    arc_buf_init(&a.code);

    char line[MAX_LINE];
    bool ok = true;
    while (fgets(line, sizeof(line), in)) {
        a.line_num++;
        /* Strip newline */
        size_t ln = strlen(line);
        if (ln > 0 && line[ln - 1] == '\n') line[ln - 1] = '\0';
        if (!assemble_line(&a, line)) { ok = false; break; }
    }
    fclose(in);

    if (!ok) {
        arc_image_free(&a.image);
        return 1;
    }

    if (!resolve_fixups(&a)) {
        arc_image_free(&a.image);
        return 1;
    }

    /* Transfer code to image */
    a.image.code = a.code.data;
    a.image.code_len = (uint32_t)a.code.len;

    /* Serialize to .mgc */
    uint8_t* out_bytes;
    size_t out_len;
    ArcStatus s = arc_image_write(&a.image, &out_bytes, &out_len);
    if (s != ARC_OK) {
        fprintf(stderr, "error: serialization failed\n");
        return 1;
    }

    FILE* out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "error: cannot write '%s'\n", argv[2]);
        ARC_FREE(out_bytes);
        return 1;
    }
    fwrite(out_bytes, 1, out_len, out);
    fclose(out);

    printf("assembled %u bytes of code, %u constants, %u functions -> %s\n",
           a.image.code_len, a.image.constants.count, a.image.functions.count, argv[2]);

    ARC_FREE(out_bytes);
    /* Don't free image.code — it's the buf's data */
    arc_const_pool_free(&a.image.constants);
    arc_func_table_free(&a.image.functions);
    arc_debug_table_free(&a.image.debug);
    return 0;
}
