#include "nemu.h"
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

enum {
    TK_NOTYPE = 256, TK_EQ, NUM, ADD, MINUS, MUL, DIV,
    LBRACKET, RBRACKET, REG, HEX, AND, OR, NEQ
};

static struct rule {
    char *regex;
    int token_type;
} rules[] = {
    {" +", TK_NOTYPE},
    {"\\+", ADD},
    {"\\-", MINUS},
    {"\\*", MUL},
    {"\\/", DIV},
    {"\\(", LBRACKET},
    {"\\)", RBRACKET},
    {"==", TK_EQ},
    {"!=", NEQ},
    {"&&", AND},
    {"\\|\\|", OR},
    {"0[xX][0-9a-fA-F]+", HEX},
    {"[0-9]+", NUM},
    {"\\$e[abc]x", REG},
    {"\\$e[bs]p", REG},
    {"\\$e[sd]i", REG},
    {"\\$eip", REG},
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))
static regex_t re[NR_REGEX];

typedef struct token {
    int type;
    char str[32];
} Token;

Token tokens[32];
int nr_token;

void init_regex() {
    int i;
    char error_msg[128];
    int ret;
    for (i = 0; i < NR_REGEX; i++) {
        ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED | REG_ICASE);
        if (ret != 0) {
            regerror(ret, &re[i], error_msg, 128);
            assert(0 && "regex compile failed");
        }
    }
}

static bool make_token(char *e) {
    int position = 0;
    int i;
    regmatch_t pmatch;
    nr_token = 0;
    while (e[position] != '\0') {
        for (i = 0; i < NR_REGEX; i++) {
            if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
                int len = pmatch.rm_eo - pmatch.rm_so;
                switch (rules[i].token_type) {
                    case TK_NOTYPE: break;
                    case NUM: case REG: case HEX:
                        strncpy(tokens[nr_token].str, e+position, len);
                        tokens[nr_token].str[len] = '\0';
                        tokens[nr_token].type = rules[i].token_type;
                        nr_token++;
                        break;
                    default:
                        tokens[nr_token].type = rules[i].token_type;
                        tokens[nr_token].str[0] = e[position];
                        tokens[nr_token].str[1] = '\0';
                        if (len == 2) tokens[nr_token].str[1] = e[position+1];
                        tokens[nr_token].str[2] = '\0';
                        nr_token++;
                        break;
                }
                position += len;
                break;
            }
        }
        if (i == NR_REGEX) {
            printf("no match at %d\n", position);
            
            return false;
        }
    }
    return true;
}

bool judge_exp() {
    int cnt = 0;
    for (int i = 0; i < nr_token; i++) {
        if (tokens[i].type == LBRACKET) cnt++;
        else if (tokens[i].type == RBRACKET) cnt--;
        if (cnt < 0) return false;
    }
    return cnt == 0;
}

uint32_t getnum(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

int priority(int t) {
    switch(tokens[t].type) {
        case OR:    return 1;
        case AND:   return 2;
        case TK_EQ: case NEQ: return 3;
        case ADD: case MINUS: return 4;
        case MUL: case DIV:   return 5;
        default: return 0;
    }
}

bool check_parentheses(int p, int q) {
    if (tokens[p].type != LBRACKET || tokens[q].type != RBRACKET) return false;
    int cnt = 0;
    for (int i = p; i <= q; i++) {
        if (tokens[i].type == LBRACKET) cnt++;
        else if (tokens[i].type == RBRACKET) cnt--;
        if (cnt == 0 && i < q) return false;
    }
    return cnt == 0;
}

static int find_dominant_operator(int p, int q) {
    int op_pos = -1;
    int min_prio = 999; 
    int bracket = 0;

    while (p <= q) {
        if (tokens[p].str[0] == '*' || tokens[p].str[0] == '!') {
            p++;
        } else {
            break;
        }
    }

    for (int i = p; i <= q; i++) {
        if (tokens[i].type == LBRACKET) { bracket++; continue; }
        if (tokens[i].type == RBRACKET) { bracket--; continue; }
        if (bracket > 0) continue;

        if (tokens[i].str[0] == '*' || tokens[i].str[0] == '!') {
            continue;
        }

        int prio = 999;
        switch (tokens[i].type) {
            case MUL: case DIV:   prio = 3; break;
            case ADD: case MINUS: prio = 2; break;
            case TK_EQ: case NEQ: prio = 1; break; 
            case AND:             prio = 0; break; 
            case OR:              prio = -1; break;
            default: continue; 
        }

        if (prio < min_prio) {
            min_prio = prio;
            op_pos = i;
        } else if (prio == min_prio) {
            op_pos = i; 
        }
    }

    return op_pos;
}

uint32_t eval(int p, int q, bool *success) {
    if (p > q) { 
        *success = false; 
        return 0; 
    }
    if (p == q) {
        if (tokens[p].type == NUM) return atoi(tokens[p].str);
        if (tokens[p].type == HEX) {
            uint32_t sum = 0;
            int len = strlen(tokens[p].str);
            for (int i = 2; i < len; i++) sum = sum*16 + getnum(tokens[p].str[i]);
            return sum;
        }
        if (tokens[p].type == REG) {
            if (!strcmp(tokens[p].str,"$eax"))return cpu.eax;
            if (!strcmp(tokens[p].str,"$ebx"))return cpu.ebx;
            if (!strcmp(tokens[p].str,"$ecx"))return cpu.ecx;
            if (!strcmp(tokens[p].str,"$edx"))return cpu.edx;
            if (!strcmp(tokens[p].str,"$ebp"))return cpu.ebp;
            if (!strcmp(tokens[p].str,"$esp"))return cpu.esp;
            if (!strcmp(tokens[p].str,"$esi"))return cpu.esi;
            if (!strcmp(tokens[p].str,"$edi"))return cpu.edi;
            if (!strcmp(tokens[p].str,"$eip"))return cpu.eip;
        }
        *success = false;
        return 0;
    }
    if (check_parentheses(p,q)) {
        return eval(p+1, q-1, success);
    }
    int op = find_dominant_operator(p,q);
    if (op == -1) {
        if (tokens[p].str[0] == '*') {
            uint32_t addr = eval(p + 1, q, success);
            if (!*success) return 0;
            return vaddr_read(addr, 4);
        } else if (tokens[p].str[0] == '!') {
            uint32_t val = eval(p + 1, q, success);
            if (!*success) return 0;
            return !val;
        } else {
            *success = false;
            return 0;
        }
    }
    uint32_t l = eval(p, op-1, success);
    uint32_t r = eval(op+1, q, success);
    if (!*success) return 0;
    switch(tokens[op].type) {
        case ADD: return l + r;
        case MINUS: return l - r;
        case MUL: return l * r;
        case DIV:
            if (r == 0) { 
                printf("Error: div zero\n"); 
                *success = false; 
                return 0; 
            }
            return l / r;
        case AND: return l && r;
        case OR: return l || r;
        case TK_EQ: return l == r;
        case NEQ: return l != r;
        default: 
            printf("Error: unknown operator %d\n", tokens[op].type);
            *success = false; 
            return 0;
    }
}

uint32_t expr(char *e, bool *success) {
    *success = true;
    if (!make_token(e)) { 
        *success = false; 
        return 0; 
    }
    if (!judge_exp()) { 
        *success = false; 
        return 0; 
    }
    return eval(0, nr_token-1, success);
}
