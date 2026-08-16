#ifndef ARCANA_VERIFIER_H
#define ARCANA_VERIFIER_H

#include "../bytecode/format.h"
#include "../bytecode/opcodes.h"

#define ARC_VERIFY_MAX_ERRORS 32

typedef struct {
    char     message[256];
    uint16_t func_idx;
    uint32_t offset;
} ArcVerifyError;

typedef struct {
    ArcVerifyError errors[ARC_VERIFY_MAX_ERRORS];
    int            error_count;
    bool           valid;
} ArcVerifyResult;

/* Verify a bytecode image. Returns result with errors if any. */
ArcVerifyResult arc_verify(const ArcBytecodeImage* image);

#endif /* ARCANA_VERIFIER_H */
