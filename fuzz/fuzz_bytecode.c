/*
 * Fuzz target: bytecode loader and verifier.
 *
 * Feed random bytes as a .mgc file to the deserializer and verifier.
 * Malformed input must never cause crashes, only structured errors.
 *
 * Build with: clang -fsanitize=fuzzer,address -o fuzz_bytecode \
 *             fuzz/fuzz_bytecode.c src/bytecode/format.c \
 *             src/bytecode/disassembler.c src/verifier/verifier.c \
 *             -Isrc -DARCANA_FUZZ
 *
 * Run: ./fuzz_bytecode corpus/ -max_len=4096
 */

#include "../src/bytecode/format.h"
#include "../src/verifier/verifier.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    /* Try to deserialize */
    ArcBytecodeImage image;
    arc_image_init(&image);

    ArcStatus st = arc_image_read(&image, data, (uint32_t)size);
    if (st != ARC_OK) {
        arc_image_free(&image);
        return 0;
    }

    /* If deserialization succeeded, run the verifier */
    ArcVerifyResult vr = arc_verify(&image);
    (void)vr; /* result doesn't matter — just must not crash */

    arc_image_free(&image);
    return 0;
}
