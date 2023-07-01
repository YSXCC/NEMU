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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "isa.h"
#include <memory/vaddr.h>
#include <stddef.h>


static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

void info_w();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  cpu_quit();
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args) {
  char *arg = strtok(args, " ");
  if (arg == NULL) {
    cpu_exec(1);
  } else {
    int number = atoi(arg);
    cpu_exec((uint64_t)number);
  }
  
  return 0;
}

static int cmd_info(char *args) {
  if (*args == 'r') {
    isa_reg_display();
  } else if (*args == 'w') {
    info_w();
  } else {
    printf("Please input r or w !\n");
  }

  return 0;
}

static int cmd_x(char *args) {
  char *pend;

  int size = strtol(args, &pend, 10);
  vaddr_t addr = strtol(pend, NULL, 16);
  
  for (int i = 0; i < size; i++) {
    word_t data = vaddr_read(addr + i * 4, 4);
    printf("0x%08x  " , addr + i * 4 );
    for(int j =0 ; j < 4 ; j++){
      printf("0x%02x " , data & 0xff);
      data = data >> 8 ;
    }
    printf("\n");
  }
  return 0;
}

static int cmd_p(char *args) {
  bool success;
  word_t res = expr(args, &success);
  if (!success) {
    puts("invalid expression");
  } else {
    printf("%u\n", res);
  }
  return 0;
}

static int cmd_w(char *args) {
  bool success = true;
  WP *tmp_wp = new_wp();
  strcpy(tmp_wp->expr, args);
  tmp_wp->in_val = expr(args, &success);
  printf("Watchpoint[%d] is set up on %s now.\n",tmp_wp->NO, tmp_wp->expr);
  return 0;
}

static int cmd_d(char *args) {
  if (args == NULL) {
    printf("Need the NO of the watchpoint!\n");
  } else {
    int n;
    sscanf(args, "%d", &n);
    free_wp(n);
  }
  return 0;
}

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "si", "Let the program pause execution after stepping into an instruction", cmd_si },
  { "info", "Print registers status(info r) or watchpoints info(info w)", cmd_info},
  { "x", "Print address memory", cmd_x},
  { "p", "Find the value of the expression", cmd_p},
  { "w", "Set up a new watchpoint", cmd_w},
  { "d", "Free the Watchpoint", cmd_d},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
