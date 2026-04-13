#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <stdio.h>

void cpu_exec(uint64_t);
void init_regex();
void display_wp();
void insert_wp(char *args);
void delete_wp(int no);
uint32_t expr(char *e, bool *success);
uint32_t vaddr_read(vaddr_t addr, int len);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
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
    return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);

static struct {
    char *name;
    char *description;
    int (*handler) (char *);
} cmd_table [] = {
    { "help", "Display informations about all supported commands", cmd_help },
    { "c", "Continue the execution of the program", cmd_c },
    { "q", "Exit NEMU", cmd_q },
    { "si", "Let the program execute n steps", cmd_si },
    { "info", "Display the register status and the watchpoint information", cmd_info},
    { "x", "Caculate the value of expression and display the content of the address", cmd_x},
    { "p","Calculate an expression", cmd_p},
    { "w", "Create a watchpoint", cmd_w},
    { "d", "Delete a watchpoint", cmd_d},
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
    char *arg = strtok(NULL, " ");
    int i;
    if (arg == NULL) {
        for (i = 0; i < NR_CMD; i ++) {
            printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        }
    } else {
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

static int cmd_si(char *args) {
    int steps = 1;
    if (args != NULL) {
        char *step_str = strtok(NULL, " ");
        if (step_str != NULL) {
            steps = atoi(step_str);
            if (steps <= 0) {
                printf("Error: steps must be positive integer\n");
                return 0;
            }
        }
    }
    cpu_exec(steps);
    return 0;
}

static int cmd_info(char *args) {
    if (args == NULL) {
        printf("Please input: info r (reg) / info w (watchpoint)\n");
    } else if (strcmp(args, "r") == 0) {
        printf("eax:  0x%-10x    %-10d\n", cpu.eax, cpu.eax);
        printf("edx:  0x%-10x    %-10d\n", cpu.edx, cpu.edx);
        printf("ecx:  0x%-10x    %-10d\n", cpu.ecx, cpu.ecx);
        printf("ebx:  0x%-10x    %-10d\n", cpu.ebx, cpu.ebx);
        printf("ebp:  0x%-10x    %-10d\n", cpu.ebp, cpu.ebp);
        printf("esi:  0x%-10x    %-10d\n", cpu.esi, cpu.esi);
        printf("esp:  0x%-10x    %-10d\n", cpu.esp, cpu.esp);
        printf("eip:  0x%-10x    %-10d\n", cpu.eip, cpu.eip);
        printf("edi:  0x%-10x    %-10d\n", cpu.edi, cpu.edi);
    } else if (strcmp(args, "w") == 0) {
        print_wp();
    } else {
        printf("Error: info only support r/w\n");
    }
    return 0;
}

static int cmd_x(char *args) {
    if (args == NULL) {
        printf("Usage: x [count] [expr]\n");
        return 0;
    }
    char *num_str = strtok(NULL, " ");
    char *addr_exp = strtok(NULL, " ");
    if (num_str == NULL || addr_exp == NULL) {
        printf("Usage: x [count] [expr]\n");
        return 0;
    }
    int num = atoi(num_str);
    if (num <= 0) {
        printf("Error: count must be positive\n");
        return 0;
    }
    bool success;
    vaddr_t addr = expr(addr_exp, &success);
    if (!success) {
        printf("Error: invalid address expr\n");
        return 0;
    }
    for (int i = 0; i < num; i++) {
        printf("0x%08x: 0x%08x\n", addr, vaddr_read(addr, 4));
        addr += 4;
    }
    return 0;
}

static int cmd_p(char *args) {
    if (args == NULL) {
        printf("Usage: p [expression]\n");
        return 0;
    }
    init_regex();
    bool success = true;
    uint32_t result = expr(args, &success);
    if (success) {
        printf("result = 0x%08x (%u)\n", result, result);
    } else {
        printf("Error: invalid expression!\n");
    }
    return 0;
}

static int cmd_w(char *args) {
    if (args == NULL) {
        printf("Usage: w [expression]\n");
        return 0;
    }
    insert_wp(args);
    return 0;
}

static int cmd_d(char *args) {
    if (args == NULL) {
        printf("Usage: d [watchpoint NO.]\n");
        return 0;
    }
    int no;
    if (sscanf(args, "%d", &no) != 1 || no <= 0) {
        printf("Error: invalid watchpoint NO.\n");
        return 0;
    }
    delete_wp(no);
    return 0;
}

void ui_mainloop(int is_batch_mode) {
    if (is_batch_mode) {
        cmd_c(NULL);
        return;
    }
    while (1) {
        char *str = rl_gets();
        if (str == NULL) break;
        char *str_end = str + strlen(str);
        char *cmd = strtok(str, " ");
        if (cmd == NULL) { continue; }
        char *args = cmd + strlen(cmd) + 1;
        if (args >= str_end) args = NULL;

#ifdef HAS_IOE
        extern void sdl_clear_event_queue(void);
        sdl_clear_event_queue();
#endif

        int i;
        for (i = 0; i < NR_CMD; i ++) {
            if (strcmp(cmd, cmd_table[i].name) == 0) {
                if (cmd_table[i].handler(args) < 0) {
                    
                    return;
                }
                break;
            }
        }
        if (i == NR_CMD) printf("Unknown command '%s'\n", cmd);
        
    }
}
