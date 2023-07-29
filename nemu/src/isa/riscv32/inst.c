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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

#ifdef CONFIG_FTRACE
  #include <ftrace.h>

  const char *get_func_name(vaddr_t addr) {
    for (uint32_t i = 0; i < num_functions; ++i) {
      if (functions[i].start_addr <= addr && addr < functions[i].start_addr + functions[i].size)
        return functions[i].name;
      }
    return NULL;
  }

  static uint32_t print_tabnum = 0;

  void print_tab() {
    printf(":");
    for (int i = 0; i < print_tabnum; i++){
      printf(" ");
    }
  }
#endif



#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

enum {
  TYPE_I, TYPE_U, TYPE_S, TYPE_J, TYPE_R, TYPE_B, TYPE_SH,
  TYPE_N, // none
};

void csrrwrs(word_t rd, word_t src1, word_t imm, bool temp){
  word_t t, *ptr = &gpr(0);
  switch (imm) {
    case 0x300: ptr = &cpu.csr.mstatus; break;// mstatus
    case 0x305: ptr = &cpu.csr.mtvec;   break;// mtvec
    case 0x341: ptr = &cpu.csr.mepc;    break;// mepc
    case 0x342: ptr = &cpu.csr.mcause;  break;// mcause
  }
  t = *ptr;
  if (temp) {
    *ptr = src1;
  } else {
    *ptr = t | src1;
  }
  gpr(rd) = t;
}

#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
#define immJ() do { *imm = (SEXT(BITS(i, 31, 31), 1) << 20) | (BITS(i, 30, 21) << 1) | (BITS(i, 20, 20) << 11) |(BITS(i, 19, 12) << 12);} while(0)
#define immB() do { *imm = (SEXT(BITS(i, 31, 31), 1) << 12) | (BITS(i, 30, 25) << 5) | (BITS(i, 11, 8) <<   1) |(BITS(i,  7,  7) << 11);} while(0)
#define immSH()   { *imm = rs2;} //shamt

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst.val;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_J:                   immJ(); break;
    case TYPE_R: src1R(); src2R();       ; break;
    case TYPE_B: src1R(); src2R(); immB(); break;
    case TYPE_SH:src1R(); src2R();immSH(); break;
  }
}

static int decode_exec(Decode *s) {
  int rd = 0;
  word_t src1 = 0, src2 = 0, imm = 0;
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

#ifdef CONFIG_FTRACE

#define FTRACEJAL(s) { \
  vaddr_t now_pc = s->pc;\
  vaddr_t next_pc = imm + s->pc;\
  const char *name =  get_func_name(next_pc);\
  if (name != NULL) {\
    printf(FMT_PADDR, now_pc);\
    print_tab();\
    printf(" call [%s@" FMT_PADDR "]\n", name, next_pc);\
  } else {\
    printf(FMT_PADDR, now_pc);\
    print_tab();\
    printf(" call [???@" FMT_PADDR "]\n", next_pc);\
  }\
  print_tabnum++;\
}

#define FTRACEJALR(s) { \
  vaddr_t now_pc = s->pc;\
  vaddr_t next_pc = (src1 + imm) & (~1);\
  uint32_t i = s->isa.inst.val;\
  int rs1 = BITS(i, 19, 15);\
  if(rd == 0 && rs1 == 1) { \
    const char *name = get_func_name(now_pc); \
    if (name != NULL) { \
      printf(FMT_PADDR,now_pc);\
      print_tab();\
      printf("ret  [%s]\n", name);\
    } else {\
      printf(FMT_PADDR,now_pc);\
      print_tab();\
      printf("ret  [???]\n");\
    }\
    print_tabnum--;\
  } else { \
    const char *name = get_func_name(next_pc);\
    if (name != NULL) {\
      printf(FMT_PADDR, now_pc);\
      print_tab();\
      printf(" call [%s@" FMT_PADDR "]\n", name, next_pc);\
    } else {\
      printf(FMT_PADDR, now_pc);\
      print_tab();\
      printf(" call [???@" FMT_PADDR "]\n", next_pc);\
    }\
    print_tabnum++;\
  }\
}
#endif


  INSTPAT_START();
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm);
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = imm + s->pc);

  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = Mr(src1 + imm, 4));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, R(rd) = SEXT(Mr(src1 + imm, 2), 16));
  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb     , I, R(rd) = SEXT(Mr(src1 + imm, 1), 8));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, R(rd) = Mr(src1 + imm, 2));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm);
#ifdef CONFIG_FTRACE
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, FTRACEJALR(s); s->dnpc = (src1 + imm) & (~1); R(rd) = s->pc + 4);
#else
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, s->dnpc = (src1 + imm) & (~1); R(rd) = s->pc + 4);
#endif
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = (src1  < imm ? 1 : 0));
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli   ,SH, R(rd) = (src1 << imm));
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = (src1  & imm));
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   ,SH, R(rd) = (src1 >> imm));
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   ,SH, R(rd) = ((sword_t)(src1) >> (sword_t)imm));
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1 ^ imm);
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori    , I, R(rd) = src1 | imm);
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti   , I, R(rd) = (sword_t)src1 < (sword_t)imm ? 1 : 0);
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw	 , I, csrrwrs(rd, src1, imm, true);IFDEF(CONFIG_ETRACE, Log("etrace: csrrs")));
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs	 , I, csrrwrs(rd, src1, imm, false);IFDEF(CONFIG_ETRACE, Log("etrace: csrrs")));
  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall	 , I, s->dnpc = isa_raise_intr(cpu.gpr[17], s->pc);IFDEF(CONFIG_ETRACE, Log("etrace: ecall")));

  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, src2));
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, src2));

#ifdef CONFIG_FTRACE
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, s->dnpc = s->pc + imm; R(rd) = s->pc + 4; FTRACEJAL(s));
#else
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, s->dnpc = s->pc + imm; R(rd) = s->pc + 4);
#endif

  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, if ((sword_t)(src1) >= (sword_t)(src2)){s->dnpc = imm + s->pc;});
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, if(src1 >=src2){s->dnpc = imm + s->pc;});
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, if((sword_t)(src1) < (sword_t)(src2)){s->dnpc = imm + s->pc;});
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, if(src1 < src2){s->dnpc = imm + s->pc;});
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, if(src1 != src2) {s->dnpc = imm + s->pc;});
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, if(src1 == src2) {s->dnpc = imm + s->pc;});


  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = src1 + src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = src1 - src2);
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, R(rd) = src1 * src2);
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh   , R, R(rd) = (sword_t)((SEXT((long long)src1, 32) * SEXT((long long)src2, 32)) >> 32));
  INSTPAT("0000001 ????? ????? 011 ????? 01100 11", mulhu  , R, R(rd) = ((long long)src1 * (long long)src2) >> 32);
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, R(rd) = (sword_t)src1/(sword_t)src2);
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11", divu   , R, R(rd) = src1 / src2);
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, R(rd) = (sword_t)src1 % (sword_t)src2);
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11", remu   , R, R(rd) = src1 % src2);
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = (sword_t)src1 >> (sword_t)src2);
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = (src1 >> src2));
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = (src1 << src2));
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, R(rd) = ((sword_t)src1 < (sword_t)src2 ? 1 : 0));
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = src1 < src2 ? 1 : 0);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = (src1 ^ src2));
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = (src1 | src2));
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1 & src2);

  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret	 , N, s->dnpc = cpu.csr.mepc+4;IFDEF(CONFIG_ETRACE, Log("etrace: mret")));
  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0
  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
