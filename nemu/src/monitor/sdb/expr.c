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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <memory/vaddr.h>

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_DEC, 
  TK_HEX, TK_REG, TK_NEQ, TK_AND, TK_OR,
  DEREF
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},              // spaces
  {"\\+", '+'},                   // plus
  {"==", TK_EQ},                  // equal
  {"0[xX][0-9a-fA-F]+", TK_HEX},  // hex
  {"[0-9]+", TK_DEC},             // dec
  {"\\*", '*'},                   // mul
  {"/", '/'},                     // div
  {"\\(", '('},                   // bra1
  {"\\)", ')'},                   // bra2
  {"-", '-'},                     // sub
  {"\\$((0)|(ra)|(sp)|(gp)|(tp)|(t0)|(t1)|(t2)|(s0)|(s1)|(a0)|(a1)|(a2)|(a3)|(a4)|(a5)|(a6)|(a7)|(s2)|(s3)|(s4)|(s5)|(s6)|(s7)|(s8)|(s9)|(s10)|(s11)|(t3)|(t4)|(t5)|(t6))", TK_REG},
  {"!=", TK_NEQ},                 // not equal
  {"&&", TK_AND},                 // and
  {"\\|\\|", TK_OR},              // or
  {""}

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case '+':
          case '-':
          case ')':
          case '(':
          case '/':
          case '*':
          case TK_DEC:
          case TK_REG:
          case TK_EQ:
          case TK_NEQ:
          case TK_AND:
          case TK_OR:
          case TK_HEX:
            tokens[nr_token].type = rules[i].token_type;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          case TK_NOTYPE:
            break;
	        default: TODO();
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

bool check_parentheses(int p, int q) {
  if (tokens[p].type == '(' && tokens[q].type == ')') {
    int par = 0;
    for (int i = p; i <= q; i++) {
      if (tokens[i].type == '(') {
        par++;
      } else if (tokens[i].type == ')') {
        par--;
      }
      if (par == 0) {
        return i == q;
      }
    }
  }
  return false;
}

int find_major(int p, int q) {
  int ret = -1;
  int prior = 0;
  for (int i = p; i <= q; i++) {
    switch (tokens[i].type) {
    case TK_AND:
    case TK_OR:
      prior = 5;
      ret = i;
      break;
    case TK_EQ:
    case TK_NEQ:
      if (prior > 4) {
        break;
      } else {
        prior = 4;
        ret = i;
      }
    case '+':
    case '-':
      if (prior > 3) {
        break;
      } else {
        prior = 3;
        ret = i;
      }
    case '*':
    case '/':
      if (prior > 2) {
        break;
      } else {
        prior = 2;
        ret = i;
      }
      break;
    case DEREF:
      if (prior > 1) {
        break;
      } else {
        prior = 1;
        ret = i;
      }
      break;
    case '(':
      do {
        i++;
      } while (tokens[i].type != ')' && i <= q);
      break;
    default:
      break;
    }
  }
  return ret;
}


word_t eval(int p, int q, bool *ok) {
  *ok = true;
  if (p > q) {
    /* Bad expression */
    *ok = false;
    return 0;
  } else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    word_t ret;
    switch (tokens[p].type) {
    case TK_DEC:
      ret = strtol(tokens[p].str, NULL, 10);
      return ret;
    case TK_HEX:
      ret = strtol(tokens[p].str, NULL, 16);
      return ret;
    case TK_REG:
      ret = isa_reg_str2val(tokens[p].str, ok);
      return ret;
    default:
      *ok = false;
      return 0;
    }
  } else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1, ok);
  } else {
    /* We should do more things here. */
    int op = find_major(p, q);

    bool ok;
    vaddr_t addr;
    int result;
    switch (tokens[op].type) {
      case DEREF:
        addr = eval(p + 1, q, &ok);
        result = vaddr_read(addr,4);
        return result;
      default:
        break;
    }

    int val1 = eval(p, op - 1, &ok);
    int val2 = eval(op + 1, q, &ok);

    switch(tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': return val1 / val2;
      case TK_NEQ: return (val1 != val2);
      case TK_EQ:  return (val1 == val2);
      case TK_AND: return (val1 && val2);
      case TK_OR:  return (val1 || val2);
      default: assert(0);
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '*') {
      if ((i == 0) || (tokens[i - 1].type != TK_DEC && tokens[i - 1].type != ')')) {
        tokens[i].type = DEREF;
      }
    }
  }
  *success = true;
  return eval(0, nr_token - 1, success);
}
