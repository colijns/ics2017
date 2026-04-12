#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char exp[128];         // 存储观察点表达式
  uint32_t value;        // 存储表达式上一次的值

} WP;

void init_wp_pool();
void insert_wp(char *args);
void delete_wp(int no);
void display_wp();
int haschanged(int *changed_no, int max_len);

#endif
