/*
 * Fuzz target: semantic graph fixture parser.
 *
 * Feed random text as .graph fixtures. Malformed input must produce
 * structured parse errors, never crashes.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_fixture \
 *             fuzz/fuzz_fixture.c src/semantic_graph/semantic_graph.c \
 *             src/semantic_graph/fixture_parser.c \
 *             -Isrc -DARCANA_FUZZ
 *
 * Run: ./fuzz_fixture corpus/ -max_len=4096
 */

#include "../src/semantic_graph/fixture_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    /* Null-terminate the input */
    char* text = (char*)malloc(size + 1);
    if (!text) return 0;
    memcpy(text, data, size);
    text[size] = '\0';

    ArcFixtureResult result = arc_fixture_parse(text);

    if (result.success)
        arc_graph_free(&result.graph);

    free(text);
    return 0;
}
