#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define NR_WP 32
static WP wp_pool[NR_WP];
static WP *head, *free_;

WP* new_wp();
void free_wp(WP *wp);

void init_wp_pool() {
    int i;
    for (i = 0; i < NR_WP; i ++) {
        wp_pool[i].NO = i;
        wp_pool[i].next = &wp_pool[i + 1];
        wp_pool[i].exp[0] = '\0';
        wp_pool[i].value = 0;
    }
    wp_pool[NR_WP - 1].next = NULL;
    head = NULL;
    free_ = wp_pool;
}

WP* new_wp() {
    assert(free_ != NULL && "Too many watchpoints!");
    WP *wp = free_;
    free_ = free_->next;
    wp->next = NULL;
    return wp;
}

void free_wp(WP *wp) {
    memset(wp->exp, 0, sizeof(wp->exp));
    wp->value = 0;
    wp->next = free_;
    free_ = wp;
}

void insert_wp(char *args) {
    bool success = true;

    uint32_t val = expr(args, &success);
    if (!success) {
        printf("Error: invalid expr, create wp failed!\n");
        return;
    }
    WP *wp = new_wp();
    strncpy(wp->exp, args, sizeof(wp->exp)-1);
    wp->value = val;
    wp->hitNum=0;
    if (head == NULL) {
        wp->NO = 1;
        head = wp;
    } else {
        WP *p = head;
        while (p->next) p = p->next;
        wp->NO = p->NO + 1;
        p->next = wp;
    }
    printf("Watchpoint %d created: %s\n", wp->NO, args);
}

void delete_wp(int no) {
    if (head == NULL) {
        printf("No watchpoint to delete!\n");
        return;
    }
    WP *p = head;
    if (head->NO == no) {
        head = head->next;
        free_wp(p);
        printf("Watchpoint %d deleted\n", no);
        return;
    }
    while (p->next && p->next->NO != no) {
        p = p->next;
    }
    if (p->next == NULL) {
        printf("Watchpoint %d not found\n", no);
        return;
    }
    WP *del = p->next;
    p->next = del->next;
    free_wp(del);
    printf("Watchpoint %d deleted\n", no);
}

void display_wp() {
    if (head == NULL) {
        printf("No watchpoint\n");
        return;
    }
    printf("NO\tEXPR\t\tVALUEO\t\tHIT TIMES\n");
    WP *p = head;
    while (p) {
        printf("%d\t%s\t\t0x%08x\t%d\n", p->NO, p->exp, p->value, p->hitNum); 
        p = p->next;
    }
}

int haschanged(int *changed_no, int max_len) {
    if (changed_no != NULL) {
        memset(changed_no, -1, sizeof(int)*max_len);
    }

    if (head == NULL) return 0;
    WP *p = head;
    int idx = 0;
    bool success;

    while (p && idx < max_len-1) {
        success = true;
        uint32_t new_val = expr(p->exp, &success);
        if (success && new_val != p->value) {
            printf("\n>>> Watchpoint %d triggered: %s\n", p->NO, p->exp);
            printf("    Old value: 0x%08x\n", p->value);
            printf("    New value: 0x%08x\n", new_val);
            
            p->value = new_val;
            p->hitNum++;
            if (changed_no != NULL) {
                changed_no[idx++] = p->NO;
            }
        }
        p = p->next;
    }
    if (changed_no != NULL) {
        changed_no[idx] = -1;
    }
    return idx;
}