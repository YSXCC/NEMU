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

#include "sdb.h"

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp() {
  if (free_ == NULL) {
    printf("There is no free wathpoint now!");
		assert(0);
  }

  WP *temp;
  temp = free_;
  free_ = free_->next;
  temp->next = NULL;
  if (head == NULL) {
    head = temp;
  } else {
    WP *temp2;
    temp2 = head;
    while (temp2->next != NULL) {
      temp2 = temp2->next;
    }
    temp2->next = temp;
  }
  return temp;
}

WP* get_wp(int n) {
  WP *wp = head;
  while (wp != NULL) {
    if (wp->NO == n) {
      return wp;
    }
    wp = wp->next;
  }
  return NULL;
}

void free_wp(int n) {
  WP *wp = get_wp(n);
  if (wp == NULL) {
    assert(0);
  }
  if (wp == head) {
    head = head->next;
  } else {
    WP *temp = head;
    while (temp != NULL && temp->next != wp) {
      temp = temp->next;
    }
    temp->next = temp->next->next;
  }
  wp->next = free_;
  free_ = wp;
  wp->in_val = 0;
  memset(wp->expr, '\0', 64);
}

bool check_watchpoints() {
  WP *tmp = head;
	bool success = true;
	bool changed = false;
  word_t tmp_val;
  if (tmp == NULL) {
    printf("Watchpoint is NULL!\n");
  }
		
  while(tmp != NULL) {
    printf("Watchpoint[%d].expr = %s\n",tmp->NO,tmp->expr);
    tmp_val = expr(tmp->expr, &success);
    if (tmp->in_val != tmp_val) {
      changed = true;
      printf("watchpoint %d has changed, from 0x%x to 0x%x\n", tmp->NO, tmp->in_val, tmp_val);
      tmp->in_val = tmp_val;
    }
    tmp = tmp->next;
  }
  return changed;
}

void info_w() {
  WP *tmp_p1 = head;
  if (tmp_p1 == NULL) {
    printf("There are no watchpoints in use!\n");
    return;
  }
  while (tmp_p1 != NULL) {
    printf("Watchpoint[%d] = %u (0x%x)\n",tmp_p1->NO,tmp_p1->in_val,tmp_p1->in_val);
    tmp_p1 = tmp_p1->next;
  }
}