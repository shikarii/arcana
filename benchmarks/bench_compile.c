/*
 * Benchmark: compilation throughput.
 *
 * Measures time to compile a semantic graph fixture N times.
 * Run: ./bench_compile [iterations]
 */

#include "../src/semantic_graph/semantic_graph.h"
#include "../src/semantic_graph/fixture_parser.h"
#include "../src/compiler/compiler.h"
#include <time.h>

static const char* GRAPH_5_PLUS_10 =
    "region r0 module\n"
    "node n0 const_int(5) in r0\n"
    "node n1 const_int(10) in r0\n"
    "node n2 add in r0 ports=[lhs,rhs,out] cyclic=[lhs,rhs,out]\n"
    "edge e0 n0.out -> n2.lhs\n"
    "edge e1 n1.out -> n2.rhs\n"
    "root n2.out\n";

int main(int argc, char** argv) {
    int iterations = 10000;
    if (argc > 1) iterations = atoi(argv[1]);
    if (iterations < 1) iterations = 1;

    printf("Compiling %d iterations...\n", iterations);

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        ArcFixtureResult fr = arc_fixture_parse(GRAPH_5_PLUS_10);
        if (!fr.success) { fprintf(stderr, "parse failed\n"); return 1; }

        ArcCompileResult cr = arc_compile(&fr.graph);
        arc_compile_result_free(&cr);
        arc_graph_free(&fr.graph);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("%.3f sec total, %.3f us/compile\n",
           elapsed, (elapsed / iterations) * 1e6);
    return 0;
}
