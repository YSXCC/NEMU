#ifndef __ELF_H__
#define __ELF_H__

#include <common.h>

typedef struct {
    vaddr_t value;
    uint32_t size;
    uint32_t string_index;
    char name[256];
} elf_func_symbol;

extern elf_func_symbol func_symbol[128];
extern uint32_t func_symbol_number;

#endif