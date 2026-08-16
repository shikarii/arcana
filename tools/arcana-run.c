/*
 * arcana-run — Load and execute a .mgc bytecode file.
 *
 * Usage: arcana-run <file.mgc> [--trace]
 */

#include "../src/common/arcana_common.h"
#include "../src/bytecode/format.h"
#include "../src/vm/vm.h"
#include "../src/verifier/verifier.h"

static uint8_t* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = ARC_ALLOC(uint8_t, (size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out_len = rd;
    return buf;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: arcana-run <file.mgc> [--trace]\n");
        return 1;
    }

    bool trace = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) trace = true;
    }

    size_t len;
    uint8_t* data = read_file(argv[1], &len);
    if (!data) {
        fprintf(stderr, "error: cannot read '%s'\n", argv[1]);
        return 1;
    }

    ArcBytecodeImage img;
    ArcStatus s = arc_image_read(data, len, &img);
    ARC_FREE(data);
    if (s != ARC_OK) {
        fprintf(stderr, "error: invalid .mgc format\n");
        return 1;
    }

    /* Verify before execution */
    ArcVerifyResult vr = arc_verify(&img);
    if (!vr.valid) {
        fprintf(stderr, "verification failed:\n");
        for (int i = 0; i < vr.error_count; i++)
            fprintf(stderr, "  [func %u @ %u] %s\n",
                    vr.errors[i].func_idx, vr.errors[i].offset, vr.errors[i].message);
        arc_image_free(&img);
        return 1;
    }

    ArcVm vm;
    arc_vm_init(&vm, &img);
    vm.trace = trace;

    s = arc_vm_run(&vm);
    if (s != ARC_OK) {
        fprintf(stderr, "runtime error: %s (ip=%u)\n", vm.error.message, vm.error.ip);
        arc_image_free(&img);
        return 1;
    }

    arc_image_free(&img);
    return 0;
}
