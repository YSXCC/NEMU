#include "ftrace.h"
#include <debug.h>

// Function to parse the ELF file and retrieve the symbol table
void load_elf(const char* elf_file, FunctionInfo* functions, uint32_t* num_functions) {
    uint32_t function_num = *num_functions;
    FILE *fp = fopen(elf_file, "rb");
    if (!fp) {
        Log("Error opening elf file: %s\n", elf_file);
        return;
    }

    Elf32_Ehdr elf_header;
    __attribute__((unused)) int ret;
    ret = fread(&elf_header, sizeof(Elf32_Ehdr), 1, fp);

    // Check if it's a valid ELF file for RISC-V 32-bit
    if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0 ||
        elf_header.e_ident[EI_CLASS] != ELFCLASS32 ||
        elf_header.e_machine != EM_RISCV ||
        elf_header.e_ident[EI_DATA] != ELFDATA2LSB) {
        Log("Invalid RISC-V ELF file: %s\n", elf_file);
        fclose(fp);
        return;
    }

    Elf32_Shdr section_header[elf_header.e_shnum];
    fseek(fp, elf_header.e_shoff, SEEK_SET);
    ret = fread(&section_header, sizeof(Elf32_Shdr) * elf_header.e_shnum, 1, fp);

    fseek(fp, section_header[elf_header.e_shstrndx].sh_offset, SEEK_SET);
    char* shstrtab = (char*)malloc(section_header[elf_header.e_shstrndx].sh_size);;
    ret = fread(shstrtab, section_header[elf_header.e_shstrndx].sh_size, 1, fp);
    
    uint8_t* section_symtab = NULL;
    uint8_t  numbers_sym_entry = 0;
    char* section_strtab = NULL;

    for (int i = 0; i < elf_header.e_shnum; i++) {
        Elf32_Shdr section = section_header[i];
        if (section.sh_type == SHT_SYMTAB) {
            numbers_sym_entry = section.sh_size / section.sh_entsize;
            section_symtab = (uint8_t*)malloc(section.sh_size);
            fseek(fp, section.sh_offset, SEEK_SET);
            ret = fread(section_symtab, section.sh_size, 1, fp);
        }

        if (strcmp(&shstrtab[section.sh_name], ".strtab") == 0) {
            section_strtab = (char*)malloc(section.sh_size);
            fseek(fp, section.sh_offset, SEEK_SET);
            ret = fread(section_strtab, section.sh_size, 1, fp);
        }
    }
    if (section_symtab == NULL || section_strtab == NULL) {
        return ;
    }
    
    Elf32_Sym* symtab_entries = (Elf32_Sym*)section_symtab;
    for (int i = 0; i < numbers_sym_entry; i++) {
        if ((symtab_entries[i].st_info & 0xF) == STT_FUNC) {
            strcpy(functions[function_num].name, &section_strtab[symtab_entries[i].st_name]);
            functions[function_num].size = symtab_entries[i].st_size;
            functions[function_num].start_addr = symtab_entries[i].st_value;
            function_num++;
        }
    }
    *num_functions = function_num;
}
