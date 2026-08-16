/*
 * arcana-compile — Compile a .graph fixture file to .mgc bytecode.
 *
 * Usage: arcana-compile <file.graph> [-o output.mgc]
 */

#include "../src/common/arcana_common.h"
#include "../src/semantic_graph/fixture_parser.h"
#include "../src/compiler/compiler.h"
#include "../src/verifier/verifier.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: arcana-compile <file.graph> [-o output.mgc]\n");
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            output_path = argv[++i];
    }

    /* Parse .graph fixture */
    ArcFixtureResult fr = arc_fixture_parse_file(input_path);
    if (!fr.success) {
        fprintf(stderr, "error: parse: %s\n", fr.error);
        return 1;
    }

    /* Validate graph */
    ArcGraphValidation gv = arc_graph_validate(&fr.graph);
    if (!gv.valid) {
        for (int i = 0; i < gv.count; i++)
            fprintf(stderr, "error: graph: %s\n", gv.errors[i].message);
        arc_graph_validation_free(&gv);
        arc_graph_free(&fr.graph);
        return 1;
    }
    arc_graph_validation_free(&gv);

    /* Compile */
    ArcCompileResult cr = arc_compile(&fr.graph);
    if (!cr.success) {
        for (int i = 0; i < cr.error_count; i++)
            fprintf(stderr, "error: compile: %s\n", cr.errors[i].message);
        arc_compile_result_free(&cr);
        arc_graph_free(&fr.graph);
        return 1;
    }

    /* Verify */
    ArcVerifyResult vr = arc_verify(&cr.image);
    if (!vr.valid) {
        for (int i = 0; i < vr.error_count; i++)
            fprintf(stderr, "error: verify: [func %u @ %u] %s\n",
                    vr.errors[i].func_idx, vr.errors[i].offset, vr.errors[i].message);
        arc_compile_result_free(&cr);
        arc_graph_free(&fr.graph);
        return 1;
    }

    /* Serialize to .mgc */
    uint8_t* bytes; size_t blen;
    if (arc_image_write(&cr.image, &bytes, &blen) != ARC_OK) {
        fprintf(stderr, "error: serialization failed\n");
        arc_compile_result_free(&cr);
        arc_graph_free(&fr.graph);
        return 1;
    }

    /* Determine output path */
    char default_out[512] = {0};
    if (!output_path) {
        strncpy(default_out, input_path, sizeof(default_out) - 5);
        char* dot = strrchr(default_out, '.');
        if (dot) *dot = '\0';
        strcat(default_out, ".mgc");
        output_path = default_out;
    }

    FILE* f = fopen(output_path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot write '%s'\n", output_path);
        ARC_FREE(bytes);
        arc_compile_result_free(&cr);
        arc_graph_free(&fr.graph);
        return 1;
    }
    fwrite(bytes, 1, blen, f);
    fclose(f);

    printf("OK: %s -> %s (%zu bytes)\n", input_path, output_path, blen);
    printf("  %u function(s), %u constant(s), %u bytes of code\n",
           cr.image.functions.count, cr.image.constants.count, cr.image.code_len);

    ARC_FREE(bytes);
    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
    return 0;
}
