#ifndef _FTRACE_H
#define _FTRACE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

// Structure to store function information
typedef struct {
    char name[256]; // Assuming function names won't exceed 255 characters
    Elf32_Addr start_addr;
    Elf32_Word size;
} FunctionInfo;

extern FunctionInfo functions[128];
extern uint32_t num_functions;

void load_elf(const char* elf_file, FunctionInfo* functions, uint32_t* num_functions);

#endif