#ifndef ARCANA_DISASSEMBLER_H
#define ARCANA_DISASSEMBLER_H

#include "format.h"

/* Disassemble an entire bytecode image to output stream */
void arc_disassemble(const ArcBytecodeImage* img, FILE* out);

/* Disassemble a single function */
void arc_disassemble_func(const ArcBytecodeImage* img, uint16_t func_idx, FILE* out);

#endif /* ARCANA_DISASSEMBLER_H */
