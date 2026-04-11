#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

int trans(char *e);
void cpu_exec(uint64_t);
void init_regex();
uint32_t expr(char *e, bool *success);
uint32_t vaddr_read(vaddr_t addr, int len);

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

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Let the program execute n steps", cmd_si },
  { "info", "Display the register status", cmd_info},
  { "x", "Display the content of the address", cmd_x},
  { "p","Calculate an expression", cmd_p},
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
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

static int cmd_si(char *args) {
  uint64_t steps = 1;
  if (args != NULL){
    char *num_str = strtok(NULL, " ");
    if (num_str != NULL) {
      steps = atoi(num_str);
    }
  }

  cpu_exec(steps);
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL || strcmp(args, "r") != 0) {
    printf("Usage: info r\n");
    return 0;
  }

  printf("eax:  0x%-10x    %-10d\n", cpu.eax, cpu.eax);
  printf("edx:  0x%-10x    %-10d\n", cpu.edx, cpu.edx);
  printf("ecx:  0x%-10x    %-10d\n", cpu.ecx, cpu.ecx);
  printf("ebx:  0x%-10x    %-10d\n", cpu.ebx, cpu.ebx);
  printf("ebp:  0x%-10x    %-10d\n", cpu.ebp, cpu.ebp);
  printf("esi:  0x%-10x    %-10d\n", cpu.esi, cpu.esi);
  printf("esp:  0x%-10x    %-10d\n", cpu.esp, cpu.esp);
  printf("eip:  0x%-10x    %-10d\n", cpu.eip, cpu.eip);

  return 0;
}

static int cmd_x(char *args) {
  if (args == NULL) {
    printf("Usage: x <n> <addr>\n");
    return 0;
  }

  int num, i;
  uint32_t addr;
  char *exp;

  num = atoi(strtok(NULL, " "));
  exp = strtok(NULL, " ");
  addr = trans(exp);

  for (i = 0; i < num; i++) {
    printf("0x%08x: 0x%08x\n", addr, vaddr_read(addr, 4));
    addr += 4;
  }

  return 0;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p <expression>\n");
    return 0;
  }

  init_regex();
  bool success = true;
  uint32_t result = expr(args, &success);

  if (success) {
    printf("result = 0x%08x (%d)\n", result, result);
  } else {
    printf("Invalid expression!\n");
  }

  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

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

int trans(char *e) {
  if (e == NULL || strlen(e) < 3) return 0;

  uint32_t num = 0;
  int i;

  for (i = 2; e[i] != '\0'; i++) {
    num = num * 16;
    if (e[i] >= '0' && e[i] <= '9') {
      num += e[i] - '0';
    }
  }

  return num;
}
