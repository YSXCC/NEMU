/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <memory/paddr.h>
#include <elf.h>

void init_rand();
void init_log(const char *log_file);
void init_mem();
void init_difftest(char *ref_so_file, long img_size, int port);
void init_device();
void init_sdb();
void init_disasm(const char *triple);

static void welcome() {
  Log("Trace: %s", MUXDEF(CONFIG_TRACE, ANSI_FMT("ON", ANSI_FG_GREEN), ANSI_FMT("OFF", ANSI_FG_RED)));
  IFDEF(CONFIG_TRACE, Log("If trace is enabled, a log file will be generated "
        "to record the trace. This may lead to a large log file. "
        "If it is not necessary, you can disable it in menuconfig"));
  Log("Build time: %s, %s", __TIME__, __DATE__);
  printf("Welcome to %s-NEMU!\n", ANSI_FMT(str(__GUEST_ISA__), ANSI_FG_YELLOW ANSI_BG_RED));
  printf("For help, type \"help\"\n");
  //Log("Exercise: Please remove me in the source code and compile NEMU again.");
  //assert(0);
}

#ifndef CONFIG_TARGET_AM
#include <getopt.h>

void sdb_set_batch_mode();

static char *log_file = NULL;
static char *diff_so_file = NULL;
static char *img_file = NULL;
static int difftest_port = 1234;

static char *elf_file = NULL;

elf_func_symbol func_symbol[128];
uint32_t func_symbol_number;

static void load_elf(char *img_file) {
    if (img_file == NULL) {
        Log("No ELF file is given.");
        return;
    }

    FILE *fp = fopen(img_file, "rb");
    Assert(fp, "Can not open '%s'", img_file);

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);

    uint8_t *elf = malloc(size * sizeof(uint8_t));
    Log("The ELF file is %s, size = %ld", img_file, size);

    fseek(fp, 0, SEEK_SET);

    int ret = fread(elf, size, 1, fp);
    assert(ret == 1);
    fclose(fp);


    uint32_t sector_header_index  = 0;
    uint32_t sector_header_length = 0; // the size of one section
    uint32_t sector_header_number = 0;

    for (uint32_t i = 35; i >= 32; --i)
        sector_header_index = sector_header_index * 256 + elf[i];

    for (uint32_t i = 47; i >= 46; --i)
        sector_header_length = sector_header_length * 256 + elf[i];

    for (uint32_t i = 49; i >= 48; --i)
        sector_header_number = sector_header_number * 256 + elf[i];

    uint32_t symbol_table_index = 0;
    uint32_t symbol_table_size  = 0;
    uint32_t string_table_index = 0;
    uint32_t string_table_size  = 0;

    for (uint32_t i = 0; i < sector_header_number; ++i) {
        uint32_t type = 0;
        type = type * 256 + elf[sector_header_index + i * sector_header_length + 5];
        type = type * 256 + elf[sector_header_index + i * sector_header_length + 4];

        if (type == 2) {
            symbol_table_index = symbol_table_index * 256 + elf[sector_header_index + i * sector_header_length + 19];
            symbol_table_index = symbol_table_index * 256 + elf[sector_header_index + i * sector_header_length + 18];
            symbol_table_index = symbol_table_index * 256 + elf[sector_header_index + i * sector_header_length + 17];
            symbol_table_index = symbol_table_index * 256 + elf[sector_header_index + i * sector_header_length + 16];

            symbol_table_size = symbol_table_size * 256 + elf[sector_header_index + i * sector_header_length + 23];
            symbol_table_size = symbol_table_size * 256 + elf[sector_header_index + i * sector_header_length + 22];
            symbol_table_size = symbol_table_size * 256 + elf[sector_header_index + i * sector_header_length + 21];
            symbol_table_size = symbol_table_size * 256 + elf[sector_header_index + i * sector_header_length + 20];
        } else if (type == 3) {
            string_table_index = string_table_index * 256 + elf[sector_header_index + i * sector_header_length + 19];
            string_table_index = string_table_index * 256 + elf[sector_header_index + i * sector_header_length + 18];
            string_table_index = string_table_index * 256 + elf[sector_header_index + i * sector_header_length + 17];
            string_table_index = string_table_index * 256 + elf[sector_header_index + i * sector_header_length + 16];

            string_table_size  = string_table_size * 256 + elf[sector_header_index + i * sector_header_length + 23];
            string_table_size  = string_table_size * 256 + elf[sector_header_index + i * sector_header_length + 22];
            string_table_size  = string_table_size * 256 + elf[sector_header_index + i * sector_header_length + 21];
            string_table_size  = string_table_size * 256 + elf[sector_header_index + i * sector_header_length + 20];
        }

        if (symbol_table_index && string_table_index) // assume 
            break;
    }

    func_symbol_number = 0;
    for (uint32_t i = 0; i < symbol_table_size / 16; ++i) {
        if ((elf[symbol_table_index + i * 16 + 12] & 0x0f) == 2) {
            vaddr_t value = 0;
            uint32_t size = 0;
            uint32_t string_index = 0;

            string_index = string_index * 256 + elf[symbol_table_index + i * 16 + 3];
            string_index = string_index * 256 + elf[symbol_table_index + i * 16 + 2];
            string_index = string_index * 256 + elf[symbol_table_index + i * 16 + 1];
            string_index = string_index * 256 + elf[symbol_table_index + i * 16 + 0];

            value = value * 256 + elf[symbol_table_index + i * 16 + 7];
            value = value * 256 + elf[symbol_table_index + i * 16 + 6];
            value = value * 256 + elf[symbol_table_index + i * 16 + 5];
            value = value * 256 + elf[symbol_table_index + i * 16 + 4];

            size = size * 256 + elf[symbol_table_index + i * 16 + 11];
            size = size * 256 + elf[symbol_table_index + i * 16 + 10];
            size = size * 256 + elf[symbol_table_index + i * 16 + 9];
            size = size * 256 + elf[symbol_table_index + i * 16 + 8];

            func_symbol[func_symbol_number].value = value;
            func_symbol[func_symbol_number].size = size;
            func_symbol[func_symbol_number].string_index = string_index;

            func_symbol_number++;
            if (func_symbol_number >= 128) {
                Log("Function symbol storage limit exceeded.");
                break;
            }
        }
    }

    for (uint32_t i = 0; i < func_symbol_number; ++i) {
        uint32_t string_index_st = func_symbol[i].string_index;
        uint32_t string_index_ed = string_index_st;
        while(elf[string_table_index + string_index_ed])
            string_index_ed++;
        if (string_index_ed - string_index_st + 1 > 256)
            panic("Function name is too long");
        strncpy(func_symbol[i].name, (const char *)&elf[string_table_index + string_index_st], string_index_ed - string_index_st + 1);
        Assert(func_symbol[i].name[string_index_ed - string_index_st + 1] == '\0', "The string does not terminate with a zero character.");
    }

    free(elf);
    return;
}

static long load_img() {
  if (img_file == NULL) {
    Log("No image is given. Use the default build-in image.");
    return 4096; // built-in image size
  }

  FILE *fp = fopen(img_file, "rb");
  Assert(fp, "Can not open '%s'", img_file);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  Log("The image is %s, size = %ld", img_file, size);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(guest_to_host(RESET_VECTOR), size, 1, fp);
  assert(ret == 1);

  fclose(fp);
  return size;
}

static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"elf"      , required_argument, NULL, 'e'},
    {"batch"    , no_argument      , NULL, 'b'},
    {"log"      , required_argument, NULL, 'l'},
    {"diff"     , required_argument, NULL, 'd'},
    {"port"     , required_argument, NULL, 'p'},
    {"help"     , no_argument      , NULL, 'h'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ( (o = getopt_long(argc, argv, "-bhl:d:p:e:", table, NULL)) != -1) {
    switch (o) {
      case 'e': elf_file = optarg; break;
      case 'b': sdb_set_batch_mode(); break;
      case 'p': sscanf(optarg, "%d", &difftest_port); break;
      case 'l': log_file = optarg; break;
      case 'd': diff_so_file = optarg; break;
      case 1: img_file = optarg; return 0;
      default:
        printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
        printf("\t-e,--elf                read elf file\n");
        printf("\t-b,--batch              run with batch mode\n");
        printf("\t-l,--log=FILE           output log to FILE\n");
        printf("\t-d,--diff=REF_SO        run DiffTest with reference REF_SO\n");
        printf("\t-p,--port=PORT          run DiffTest with port PORT\n");
        printf("\n");
        exit(0);
    }
  }
  return 0;
}

void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */

  /* Parse arguments. */
  parse_args(argc, argv);

  /* Set random seed. */
  init_rand();

  /* Open the log file. */
  init_log(log_file);

  /* Initialize memory. */
  init_mem();

  /* Initialize devices. */
  IFDEF(CONFIG_DEVICE, init_device());

  /* Perform ISA dependent initialization. */
  init_isa();

  /* Load the elf of image. This will help us to get function trace. */
  IFDEF(CONFIG_FTRACE, load_elf(elf_file));

  /* Load the image to memory. This will overwrite the built-in image. */
  long img_size = load_img();

  /* Initialize differential testing. */
  init_difftest(diff_so_file, img_size, difftest_port);

  /* Initialize the simple debugger. */
  init_sdb();

#ifndef CONFIG_ISA_loongarch32r
  IFDEF(CONFIG_ITRACE, init_disasm(
    MUXDEF(CONFIG_ISA_x86,     "i686",
    MUXDEF(CONFIG_ISA_mips32,  "mipsel",
    MUXDEF(CONFIG_ISA_riscv32, "riscv32",
    MUXDEF(CONFIG_ISA_riscv64, "riscv64", "bad")))) "-pc-linux-gnu"
  ));
#endif

  /* Display welcome message. */
  welcome();
}
#else // CONFIG_TARGET_AM
static long load_img() {
  extern char bin_start, bin_end;
  size_t size = &bin_end - &bin_start;
  Log("img size = %ld", size);
  memcpy(guest_to_host(RESET_VECTOR), &bin_start, size);
  return size;
}

void am_init_monitor() {
  init_rand();
  init_mem();
  init_isa();
  load_img();
  IFDEF(CONFIG_DEVICE, init_device());
  welcome();
}
#endif
