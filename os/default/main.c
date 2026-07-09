#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hal/board.h>
#include <hal/hal_gpio.h>
#include <hal/hal_sdmmc.h>
#include <hal/hal_eth.h>
#include <ttak/mem/mem.h>
#include <ttak/timing/timing.h>
#include <ttak/async/function.h>
#include <ttak/async/sched.h>
#include <os/saramos_arena.h>
#include <os/saramos_kernel.h>
#include <os/saramos_owner.h>
#include <os/saramos_scheduler.h>
#include <os/saramos_process.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "lwip/apps/httpd.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include "http_server.h"
#include "sd_diskio.h"
#include "diskio.h"
#include "ff.h"

#ifdef ENABLE_TOOL_FSUTILS
#include "fsutils.h"
#include "shell.h"
#endif
#ifdef ENABLE_TOOL_COREUTILS
#include "coreutils.h"
#endif
#ifdef ENABLE_TOOL_VI
#include "saramos_port.h"
#endif

/* lwIP sys_now() for bare-metal NO_SYS=1 mode */
u32_t sys_now(void)
{
    return (u32_t)ttak_get_tick_count();
}

static struct netif gnetif;
static int net_initialized = 0;
static int dhcp_bound = 0;

#define CMD_BUF_SIZE 64
#include "program.h"

static saramos_arena_t sys_arena;
static saramos_owner_t sys_owner;
static int arena_initialized = 0;
static int owner_initialized = 0;

cli_program_t programs[PROGRAM_MAX_COUNT];

static void cli_read_line(char *buf, size_t size);

static void cli_prompt(void)
{
    hal_uart_puts("saramOS> ");
}

static void cli_help(void)
{
    hal_uart_puts(
        "Available commands:\r\n"
        "  help       - Show this help message\r\n"
        "  status     - Show OS status (arena, owner, ticks)\r\n"
        "  alloc <n>  - Allocate n bytes from arena\r\n"
        "  reset      - Reset current arena generation\r\n"
        "  rotate     - Rotate arena to new generation\r\n"
        "  hello      - Print hello message\r\n"
        "  heartbeat  - Print heartbeat once\r\n"
        "  program    - Create/list/run calculator programs\r\n"
        "  clear      - Clear screen\r\n"
        "  net init   - Initialize Ethernet and start DHCP\r\n"
        "  net status - Show network link/IP status\r\n"
        "  net show mac  - Show Ethernet MAC address\r\n"
        "  net show ipv4 - Show IPv4 address/netmask/gateway\r\n"
        "  net show ipv6 - Show IPv6 addresses\r\n"
        "  http start - Start HTTP server\r\n"
        "  sd init    - Initialize and mount SD card\r\n"
        "  sd ls <p>  - List SD card directory\r\n"
        "  sd cat <f> - Print SD card file contents\r\n"
#ifdef ENABLE_TOOL_FSUTILS
        "  sd rm <f>  - Remove SD card file\r\n"
        "  sd mkdir <d> - Create SD card directory\r\n"
        "  sd echo [text]... - Print text to console\r\n"
        "  sd tee <f> <text>... - Write text to file\r\n"
        "  sd touch <f>... - Create files or update timestamps\r\n"
        "  sd cp <src> <dst> - Copy a file\r\n"
        "  sd mv <src> <dst> - Rename or move a file\r\n"
        "  sd chmod <mode> <f>... - Change FAT attributes\r\n"
        "  sd mountfs - Enter minimal Unix-like shell\r\n"
#ifdef ENABLE_TOOL_VI
        "  sd vi <f>  - Edit file with minimal vi\r\n"
#endif
#endif
#ifdef ENABLE_TOOL_EXTRASHELL
        "  (shell env) - cd, pwd, ls, cp, mv, rm, mkdir\r\n"
        "  (shell env) - cat, tee, head, tail, wc, grep, sort\r\n"
        "  (shell env) - find, touch, echo, env, export, source\r\n"
        "  (shell env) - if/for/test, glob/variable expansion\r\n"
#endif
        "  sd info    - Show SD card info\r\n"
        "  sd inspect - Show SD pin/register state\r\n"
        "  pin ...    - GPIO pin control\r\n"
        "  inp ...    - Sample last input pin\r\n"
    );
}

static void cli_status(void)
{
    char buf[64];

    hal_uart_puts("--- OS Status ---\r\n");

    if (arena_initialized) {
        size_t rem = saramos_arena_remaining(&sys_arena);
        snprintf(buf, sizeof(buf), "Arena: initialized, remaining=%u bytes\r\n", (unsigned)rem);
        hal_uart_puts(buf);
    } else {
        hal_uart_puts("Arena: not initialized\r\n");
    }

    if (owner_initialized) {
        hal_uart_puts("Owner: initialized (kernel)\r\n");
    } else {
        hal_uart_puts("Owner: not initialized\r\n");
    }

    snprintf(buf, sizeof(buf), "Tick count: %lu\r\n", (unsigned long)ttak_get_tick_count());
    hal_uart_puts(buf);

    hal_uart_puts("-----------------\r\n");
}

static unsigned int parse_uint(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (unsigned int)(*s - '0');
        s++;
    }
    return n;
}

static void cli_alloc(const char *arg)
{
    if (!arena_initialized) {
        hal_uart_puts("Arena not initialized.\r\n");
        return;
    }

    if (!arg || *arg == '\0') {
        hal_uart_puts("Usage: alloc <size>\r\n");
        return;
    }

    unsigned int n = parse_uint(arg);
    if (n == 0) {
        hal_uart_puts("Invalid size.\r\n");
        return;
    }

    void *p = saramos_arena_alloc(&sys_arena, (size_t)n);
    if (p) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Allocated %u bytes at %p\r\n", n, p);
        hal_uart_puts(buf);
    } else {
        hal_uart_puts("Allocation failed.\r\n");
    }
}

static void cli_reset(void)
{
    if (!arena_initialized) {
        hal_uart_puts("Arena not initialized.\r\n");
        return;
    }
    saramos_arena_reset(&sys_arena);
    hal_uart_puts("Arena reset OK\r\n");
}

static void cli_rotate(void)
{
    if (!arena_initialized) {
        hal_uart_puts("Arena not initialized.\r\n");
        return;
    }
    saramos_arena_rotate(&sys_arena);
    hal_uart_puts("Arena rotate OK\r\n");
}

static void cli_clear(void)
{
    hal_uart_puts("\x1B[2J\x1B[H");
}

static void cli_hello(void)
{
    hal_uart_puts("Hello from saramOS!\r\n");
}

static void cli_heartbeat(void)
{
    hal_uart_puts("Heartbeat from saramOS\r\n");
}

static void skip_spaces(const char **s)
{
    while (**s == ' ' || **s == '\t')
        (*s)++;
}

static int program_name_is_valid(const char *name)
{
    unsigned int len = 0;

    if (!name || *name == '\0')
        return 0;

    while (name[len] != '\0') {
        char c = name[len];
        int ok = (c >= 'a' && c <= 'z') ||
                 (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') ||
                 c == '_';
        if (!ok)
            return 0;
        len++;
    }

    return len < PROGRAM_NAME_SIZE;
}

static cli_program_t *program_find(const char *name)
{
    for (unsigned int i = 0; i < PROGRAM_MAX_COUNT; i++) {
        if (programs[i].used && strcmp(programs[i].name, name) == 0)
            return &programs[i];
    }

    return NULL;
}

cli_program_t *program_get_or_alloc(const char *name)
{
    cli_program_t *prog = program_find(name);

    if (prog)
        return prog;

    for (unsigned int i = 0; i < PROGRAM_MAX_COUNT; i++) {
        if (!programs[i].used) {
            memset(&programs[i], 0, sizeof(programs[i]));
            programs[i].used = 1;
            strncpy(programs[i].name, name, PROGRAM_NAME_SIZE - 1);
            return &programs[i];
        }
    }

    return NULL;
}

static int parse_int32(const char *s, int *out)
{
    int sign = 1;
    int value = 0;
    int digits = 0;

    skip_spaces(&s);

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
        digits++;
    }

    skip_spaces(&s);

    if (digits == 0 || *s != '\0')
        return 0;

    *out = value * sign;
    return 1;
}

static void program_list(void)
{
    char buf[64];
    int any = 0;

    hal_uart_puts("Programs:\r\n");
    for (unsigned int i = 0; i < PROGRAM_MAX_COUNT; i++) {
        if (programs[i].used) {
            snprintf(buf, sizeof(buf), "  %s (%u lines)\r\n", programs[i].name, programs[i].line_count);
            hal_uart_puts(buf);
            any = 1;
        }
    }

    if (!any)
        hal_uart_puts("  <none>\r\n");
}

static int program_find_label(const cli_program_t *prog, const char *name)
{
    for (unsigned int i = 0; i < prog->line_count; i++) {
        const char *p = prog->lines[i];
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, "label", 5) != 0)
            continue;
        p += 5;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strcmp(p, name) == 0)
            return (int)i;
    }
    return -1;
}

static int program_exec_line(const char *line, struct program_state *st, unsigned int *pc)
{
    char op[16];
    char buf[80];
    const char *arg;
    unsigned int op_len = 0;
    int value;

    skip_spaces(&line);
    if (*line == '\0') {
        (*pc)++;
        return 1;
    }

    while (line[op_len] != '\0' && line[op_len] != ' ' && line[op_len] != '\t') {
        if (op_len + 1 >= sizeof(op))
            return 0;
        op[op_len] = line[op_len];
        op_len++;
    }
    op[op_len] = '\0';
    arg = line + op_len;

    if (strcmp(op, "print") == 0) {
        skip_spaces(&arg);
        if (*arg != '\0')
            return 0;
        snprintf(buf, sizeof(buf), "ACC=%d\r\n", st->acc);
        hal_uart_puts(buf);
        (*pc)++;
        return 1;
    }

    if (strcmp(op, "input") == 0) {
        skip_spaces(&arg);
        if (*arg != '\0')
            return 0;
        hal_uart_puts("? ");
        cli_read_line(buf, sizeof(buf));
        if (!parse_int32(buf, &st->acc))
            return 0;
        (*pc)++;
        return 1;
    }

    if (strcmp(op, "inc") == 0) {
        st->acc++;
        (*pc)++;
        return 1;
    }
    if (strcmp(op, "dec") == 0) {
        st->acc--;
        (*pc)++;
        return 1;
    }
    if (strcmp(op, "neg") == 0) {
        st->acc = -st->acc;
        (*pc)++;
        return 1;
    }
    if (strcmp(op, "end") == 0 || strcmp(op, "halt") == 0) {
        *pc = st->prog->line_count;
        return 1;
    }
    if (strcmp(op, "label") == 0) {
        (*pc)++;
        return 1;
    }

    if (strcmp(op, "prints") == 0) {
        skip_spaces(&arg);
        if (*arg == '"') {
            arg++;
            size_t i = 0;
            while (arg[i] != '\0' && arg[i] != '"')
                i++;
            /* output without trailing newline if possible; keep simple */
            for (size_t j = 0; j < i; j++)
                hal_uart_putc(arg[j]);
        } else {
            hal_uart_puts(arg);
        }
        hal_uart_puts("\r\n");
        (*pc)++;
        return 1;
    }

    if (strcmp(op, "putc") == 0) {
        skip_spaces(&arg);
        if (*arg == '\0')
            return 0;
        if (*arg == '\'' && arg[1] != '\0' && arg[2] == '\'') {
            hal_uart_putc(arg[1]);
        } else {
            int c;
            if (!parse_int32(arg, &c))
                return 0;
            hal_uart_putc((char)c);
        }
        (*pc)++;
        return 1;
    }

    if (strcmp(op, "goto") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (unsigned int)target;
        return 1;
    }
    if (strcmp(op, "jeq") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (st->flag == 0) ? (unsigned int)target : (*pc + 1);
        return 1;
    }
    if (strcmp(op, "jne") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (st->flag != 0) ? (unsigned int)target : (*pc + 1);
        return 1;
    }
    if (strcmp(op, "jlt") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (st->flag < 0) ? (unsigned int)target : (*pc + 1);
        return 1;
    }
    if (strcmp(op, "jle") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (st->flag <= 0) ? (unsigned int)target : (*pc + 1);
        return 1;
    }
    if (strcmp(op, "jgt") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (st->flag > 0) ? (unsigned int)target : (*pc + 1);
        return 1;
    }
    if (strcmp(op, "jge") == 0) {
        skip_spaces(&arg);
        int target = program_find_label(st->prog, arg);
        if (target < 0)
            return 0;
        *pc = (st->flag >= 0) ? (unsigned int)target : (*pc + 1);
        return 1;
    }

    if (!parse_int32(arg, &value))
        return 0;

    if (strcmp(op, "set") == 0) {
        st->acc = value;
    } else if (strcmp(op, "add") == 0) {
        st->acc += value;
    } else if (strcmp(op, "sub") == 0) {
        st->acc -= value;
    } else if (strcmp(op, "mul") == 0) {
        st->acc *= value;
    } else if (strcmp(op, "div") == 0) {
        if (value == 0)
            return 0;
        st->acc /= value;
    } else if (strcmp(op, "mod") == 0) {
        if (value == 0)
            return 0;
        st->acc %= value;
    } else if (strcmp(op, "cmp") == 0) {
        st->flag = (st->acc < value) ? -1 : (st->acc > value) ? 1 : 0;
    } else if (strcmp(op, "store") == 0) {
        if (value < 0 || value >= PROGRAM_MEM_SIZE)
            return 0;
        st->mem[value] = st->acc;
    } else if (strcmp(op, "load") == 0) {
        if (value < 0 || value >= PROGRAM_MEM_SIZE)
            return 0;
        st->acc = st->mem[value];
    } else {
        return 0;
    }

    (*pc)++;
    return 1;
}

static void program_run(const char *name)
{
    cli_program_t *prog = program_find(name);
    struct program_state st = {0};
    char buf[80];

    if (!prog) {
        hal_uart_puts("Program not found.\r\n");
        return;
    }

    st.prog = prog;

    snprintf(buf, sizeof(buf), "Running program '%s'\r\n", prog->name);
    hal_uart_puts(buf);

    unsigned int pc = 0;
    while (pc < prog->line_count) {
        if (!program_exec_line(prog->lines[pc], &st, &pc)) {
            snprintf(buf, sizeof(buf), "Program error at line %u: %s\r\n", pc + 1, prog->lines[pc]);
            hal_uart_puts(buf);
            return;
        }
    }

    snprintf(buf, sizeof(buf), "Done. ACC=%d\r\n", st.acc);
    hal_uart_puts(buf);
}

static void program_edit(const char *name)
{
    cli_program_t *prog;
    char line[PROGRAM_LINE_SIZE];
    char buf[256];

    if (!program_name_is_valid(name)) {
        hal_uart_puts("Usage: program <name>  (letters, digits, underscore; max 15 chars)\r\n");
        return;
    }

    prog = program_get_or_alloc(name);
    if (!prog) {
        hal_uart_puts("Program storage full.\r\n");
        return;
    }

    prog->line_count = 0;
    snprintf(buf, sizeof(buf),
             "Editing '%s'.\r\n"
             "Commands:\r\n"
             "  set/add/sub/mul/div/mod <n>  inc/dec/neg/print/input\r\n"
             "  cmp <n>  jeq/jne/jlt/jle/jgt/jge <label>  goto <label>  label <name>\r\n"
             "  store/load <addr>  prints \"text\"  putc <c>  end/halt\r\n"
             "Type end to save.\r\n",
             name);
    hal_uart_puts(buf);

    while (prog->line_count < PROGRAM_MAX_LINES) {
        hal_uart_puts("prog> ");
        cli_read_line(line, sizeof(line));

        if (strcmp(line, "end") == 0) {
            snprintf(buf, sizeof(buf), "Saved '%s' (%u lines).\r\n", prog->name, prog->line_count);
            hal_uart_puts(buf);
            return;
        }

        strncpy(prog->lines[prog->line_count], line, PROGRAM_LINE_SIZE - 1);
        prog->lines[prog->line_count][PROGRAM_LINE_SIZE - 1] = '\0';
        prog->line_count++;
    }

    snprintf(buf, sizeof(buf), "Saved '%s' (%u lines, full).\r\n", prog->name, prog->line_count);
    hal_uart_puts(buf);
}

static void cli_program(const char *arg)
{
    const char *name;

    if (!arg || *arg == '\0' || strcmp(arg, "list") == 0) {
        program_list();
        hal_uart_puts("Use: program <name>, program run <name>, program list\r\n");
        return;
    }

    if (strncmp(arg, "run", 3) == 0 && (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
        name = arg + 3;
        skip_spaces(&name);
        if (!program_name_is_valid(name)) {
            hal_uart_puts("Usage: program run <name>\r\n");
            return;
        }
        program_run(name);
        return;
    }

    program_edit(arg);
}

static void cli_net_init(void)
{
    ip4_addr_t ipaddr, netmask, gw;

    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    lwip_init();
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
    netif_set_default(&gnetif);
    netif_set_up(&gnetif);
    dhcp_start(&gnetif);

    net_initialized = 1;
    dhcp_bound = 0;
    hal_uart_puts("net: initialized, DHCP requested\r\n");
}

static void cli_net_status(void)
{
    char buf[80];

    if (!net_initialized) {
        hal_uart_puts("net: not initialized (use 'net init')\r\n");
        return;
    }

    snprintf(buf, sizeof(buf), "net: link %s\r\n",
             hal_eth_link_up() ? "UP" : "DOWN");
    hal_uart_puts(buf);

    if (dhcp_supplied_address(&gnetif)) {
        snprintf(buf, sizeof(buf), "net: IP %s",
                 ip4addr_ntoa(netif_ip4_addr(&gnetif)));
        hal_uart_puts(buf);
        snprintf(buf, sizeof(buf), ", mask %s",
                 ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
        hal_uart_puts(buf);
        snprintf(buf, sizeof(buf), ", gw %s\r\n",
                 ip4addr_ntoa(netif_ip4_gw(&gnetif)));
        hal_uart_puts(buf);
    } else {
        hal_uart_puts("net: DHCP still negotiating...\r\n");
    }
}

static void cli_net_show_mac(void)
{
    char buf[64];
    unsigned int len;

    if (!net_initialized) {
        hal_uart_puts("net: not initialized (use 'net init')\r\n");
        return;
    }

    len = (gnetif.hwaddr_len > 0) ? (unsigned int)gnetif.hwaddr_len : 6U;
    if (len > sizeof(gnetif.hwaddr))
        len = sizeof(gnetif.hwaddr);

    snprintf(buf, sizeof(buf), "net: MAC %02X", (unsigned)gnetif.hwaddr[0]);
    hal_uart_puts(buf);
    for (unsigned int i = 1; i < len; i++) {
        snprintf(buf, sizeof(buf), ":%02X", (unsigned)gnetif.hwaddr[i]);
        hal_uart_puts(buf);
    }
    hal_uart_puts("\r\n");
}

static void cli_net_show_ipv4(void)
{
    char buf[80];

    if (!net_initialized) {
        hal_uart_puts("net: not initialized (use 'net init')\r\n");
        return;
    }

    if (!dhcp_supplied_address(&gnetif)) {
        hal_uart_puts("net: IPv4 not configured (DHCP still negotiating...)\r\n");
        return;
    }

    snprintf(buf, sizeof(buf), "net: IPv4 address %s\r\n",
             ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "net: IPv4 netmask %s\r\n",
             ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "net: IPv4 gateway %s\r\n",
             ip4addr_ntoa(netif_ip4_gw(&gnetif)));
    hal_uart_puts(buf);
}

static void cli_net_show_ipv6(void)
{
#if LWIP_IPV6
    char buf[80];

    if (!net_initialized) {
        hal_uart_puts("net: not initialized (use 'net init')\r\n");
        return;
    }

    int any = 0;
    for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(&gnetif, i))) {
            snprintf(buf, sizeof(buf), "net: IPv6 address %s\r\n",
                     ip6addr_ntoa(netif_ip6_addr(&gnetif, i)));
            hal_uart_puts(buf);
            any = 1;
        }
    }

    if (!any) {
        hal_uart_puts("net: IPv6 not configured\r\n");
    }
#else
    hal_uart_puts("net: IPv6 not enabled in lwIP config\r\n");
#endif
}

static void cli_http_start(void)
{
#if LWIP_HTTPD
    httpd_init();
    hal_uart_puts("http: server started\r\n");
#else
    hal_uart_puts("http: lwIP httpd not enabled\r\n");
#endif
}

static void cli_sd_init(void)
{
    FRESULT fr;
    char buf[64];

    if (!hal_sdmmc_card_present()) {
        hal_uart_puts("sd: no card detected\r\n");
        return;
    }

    fr = sd_mount();
    if (fr != FR_OK) {
        snprintf(buf, sizeof(buf), "sd: mount failed (%d)\r\n", (int)fr);
        hal_uart_puts(buf);
        return;
    }
    hal_uart_puts("sd: mounted\r\n");
}

static void cli_sd_info(void)
{
    char buf[64];
    int present = hal_sdmmc_card_present();

    snprintf(buf, sizeof(buf), "sd: card %s (PI15=%s)\r\n",
             present ? "detected" : "not detected",
             hal_gpio_read(GPIOI_BASE, 15) ? "high" : "low");
    hal_uart_puts(buf);

    if (!present)
        return;

    snprintf(buf, sizeof(buf), "sd: card %s\r\n",
             (disk_status(0) & STA_NOINIT) ? "not initialized" : "ready");
    hal_uart_puts(buf);
}

static void cli_sd_inspect(void)
{
    char buf[80];

    hal_uart_puts("SD inspect:\r\n");

    snprintf(buf, sizeof(buf), "  PI15/CD pin:     %s\r\n",
             hal_gpio_read(GPIOI_BASE, 15) ? "high" : "low");
    hal_uart_puts(buf);

    snprintf(buf, sizeof(buf), "  card_present():  %d\r\n", hal_sdmmc_card_present());
    hal_uart_puts(buf);

    snprintf(buf, sizeof(buf), "  STA_NOINIT:      %d\r\n",
             (disk_status(0) & STA_NOINIT) ? 1 : 0);
    hal_uart_puts(buf);

    snprintf(buf, sizeof(buf), "  SDMMC2 POWER:    0x%08lX\r\n", (unsigned long)SDMMC2->POWER);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  SDMMC2 CLKCR:    0x%08lX\r\n", (unsigned long)SDMMC2->CLKCR);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  SDMMC2 STA:      0x%08lX\r\n", (unsigned long)SDMMC2->STA);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  RCC AHB2RSTR:    0x%08lX\r\n", (unsigned long)RCC_AHB2RSTR);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  RCC AHB2ENR:     0x%08lX\r\n", (unsigned long)RCC_AHB2ENR);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  RCC APB2ENR:     0x%08lX\r\n", (unsigned long)RCC_APB2ENR);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  RCC DCKCFGR2:    0x%08lX\r\n", (unsigned long)RCC_DCKCFGR2);
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  GPIOB MODER:     0x%08lX AFRL: 0x%08lX PUPDR: 0x%08lX\r\n",
             (unsigned long)*(volatile uint32_t *)(GPIOB_BASE + 0x00U),
             (unsigned long)*(volatile uint32_t *)(GPIOB_BASE + 0x20U),
             (unsigned long)*(volatile uint32_t *)(GPIOB_BASE + 0x0CU));
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  GPIOD MODER:     0x%08lX AFRL: 0x%08lX\r\n",
             (unsigned long)*(volatile uint32_t *)(GPIOD_BASE + 0x00U),
             (unsigned long)*(volatile uint32_t *)(GPIOD_BASE + 0x20U));
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  GPIOG MODER:     0x%08lX AFRH: 0x%08lX PUPDR: 0x%08lX\r\n",
             (unsigned long)*(volatile uint32_t *)(GPIOG_BASE + 0x00U),
             (unsigned long)*(volatile uint32_t *)(GPIOG_BASE + 0x24U),
             (unsigned long)*(volatile uint32_t *)(GPIOG_BASE + 0x0CU));
    hal_uart_puts(buf);
    snprintf(buf, sizeof(buf), "  GPIOI MODER:     0x%08lX AFRH: 0x%08lX PUPDR: 0x%08lX\r\n",
             (unsigned long)*(volatile uint32_t *)(GPIOI_BASE + 0x00U),
             (unsigned long)*(volatile uint32_t *)(GPIOI_BASE + 0x24U),
             (unsigned long)*(volatile uint32_t *)(GPIOI_BASE + 0x0CU));
    hal_uart_puts(buf);

    snprintf(buf, sizeof(buf), "  capacity blocks: %lu\r\n",
             (unsigned long)hal_sdmmc_get_sector_count());
    hal_uart_puts(buf);
}

static void cli_sd_ls(const char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    char buf[288];
    const char *p = path && *path ? path : "/";

    fr = f_opendir(&dir, p);
    if (fr != FR_OK) {
        snprintf(buf, sizeof(buf), "sd: cannot open dir '%s' (%d)\r\n", p, (int)fr);
        hal_uart_puts(buf);
        return;
    }

    snprintf(buf, sizeof(buf), "Listing '%s':\r\n", p);
    hal_uart_puts(buf);

    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == '\0')
            break;
        snprintf(buf, sizeof(buf), "  %c %10lu %s\r\n",
                 (fno.fattrib & AM_DIR) ? 'D' : 'F',
                 (unsigned long)fno.fsize,
                 fno.fname);
        hal_uart_puts(buf);
    }

    f_closedir(&dir);
}

static void cli_sd_cat(const char *path)
{
    FIL fil;
    FRESULT fr;
    char buf[128];
    UINT br;

    if (!path || *path == '\0') {
        hal_uart_puts("Usage: sd cat <file>\r\n");
        return;
    }

    fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        snprintf(buf, sizeof(buf), "sd: cannot open '%s' (%d)\r\n", path, (int)fr);
        hal_uart_puts(buf);
        return;
    }

    while (f_read(&fil, buf, sizeof(buf) - 1, &br) == FR_OK && br > 0) {
        buf[br] = '\0';
        hal_uart_puts(buf);
    }
    hal_uart_puts("\r\n");

    f_close(&fil);
}

#ifdef ENABLE_TOOL_FSUTILS
static int cli_sd_argc(const char *arg, const char *cmdname, char *argv[], int max_argc, char *buf, size_t buf_size)
{
    if (!arg)
        return 0;
    strncpy(buf, arg, buf_size - 1);
    buf[buf_size - 1] = '\0';

    int argc = 0;
    if (cmdname)
        argv[argc++] = (char *)cmdname;

    char *p = buf;
    while (*p && argc < max_argc) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

static void cli_sd_rm(const char *arg)
{
    char buf[128];
    char *argv[8];
    int argc = cli_sd_argc(arg, "rm", argv, 8, buf, sizeof(buf));
    if (argc < 2) {
        hal_uart_puts("Usage: sd rm <file>\r\n");
        return;
    }
    saramos_rm(argc, argv);
}

static void cli_sd_mkdir(const char *arg)
{
    char buf[128];
    char *argv[8];
    int argc = cli_sd_argc(arg, "mkdir", argv, 8, buf, sizeof(buf));
    if (argc < 2) {
        hal_uart_puts("Usage: sd mkdir <dir>\r\n");
        return;
    }
    saramos_mkdir(argc, argv);
}

static void cli_sd_echo(const char *arg)
{
    char buf[128];
    char *argv[16];
    int argc = cli_sd_argc(arg, "echo", argv, 16, buf, sizeof(buf));
    saramos_echo(argc, argv);
}

static void cli_sd_tee(const char *arg)
{
    char buf[256];
    char *argv[16];
    int argc = cli_sd_argc(arg, "tee", argv, 16, buf, sizeof(buf));
    if (argc < 3) {
        hal_uart_puts("Usage: sd tee <file> <text>...\r\n");
        return;
    }
    saramos_tee(argc, argv);
}

#ifdef ENABLE_TOOL_COREUTILS
static void cli_sd_touch(const char *arg)
{
    char buf[256];
    char *argv[16];
    int argc = cli_sd_argc(arg, "touch", argv, 16, buf, sizeof(buf));
    if (argc < 2) {
        hal_uart_puts("Usage: sd touch <file>...\r\n");
        return;
    }
    saramos_touch(argc, argv);
}

static void cli_sd_cp(const char *arg)
{
    char buf[256];
    char *argv[8];
    int argc = cli_sd_argc(arg, "cp", argv, 8, buf, sizeof(buf));
    if (argc < 3) {
        hal_uart_puts("Usage: sd cp <src> <dst>\r\n");
        return;
    }
    saramos_cp(argc, argv);
}

static void cli_sd_mv(const char *arg)
{
    char buf[256];
    char *argv[8];
    int argc = cli_sd_argc(arg, "mv", argv, 8, buf, sizeof(buf));
    if (argc < 3) {
        hal_uart_puts("Usage: sd mv <src> <dst>\r\n");
        return;
    }
    saramos_mv(argc, argv);
}

static void cli_sd_chmod(const char *arg)
{
    char buf[256];
    char *argv[16];
    int argc = cli_sd_argc(arg, "chmod", argv, 16, buf, sizeof(buf));
    if (argc < 3) {
        hal_uart_puts("Usage: sd chmod <mode> <file>...\r\n");
        return;
    }
    saramos_chmod(argc, argv);
}
#endif

static void cli_sd_mountfs(void)
{
#ifdef ENABLE_TOOL_EXTRASHELL
    saramos_proc_spawn_child(saramos_current_proc, "shell",
                             shell_interactive_process,
                             0, NULL, NULL, NULL, 0);
#else
    shell_run();
#endif
}
#endif

#ifdef ENABLE_TOOL_VI
static void cli_sd_vi(const char *arg)
{
    char buf[128];
    char *argv[4];
    int argc = cli_sd_argc(arg, "vi", argv, 4, buf, sizeof(buf));
    if (argc < 2) {
        hal_uart_puts("Usage: sd vi <file>\r\n");
        return;
    }
    saramos_vi(argc, argv);
}
#endif

/* --- GPIO pin debug state --- */
static uint32_t last_input_port = 0;
static uint8_t  last_input_pin = 0;
static int      last_input_valid = 0;

static int parse_pin_name(const char *name, uint32_t *port_base, uint8_t *pin)
{
    uint32_t base;
    unsigned int p;
    const char *num;
    char port_char;

    if (!name || name[0] == '\0')
        return 0;

    /* Accept both "PA0" / "PG2" and "A0" / "G2" forms */
    if (name[0] == 'P' || name[0] == 'p') {
        if (name[1] == '\0')
            return 0;
        port_char = name[1];
        num = name + 2;
    } else {
        port_char = name[0];
        num = name + 1;
    }

    switch (port_char) {
    case 'A': case 'a': base = GPIOA_BASE; break;
    case 'B': case 'b': base = GPIOB_BASE; break;
    case 'C': case 'c': base = GPIOC_BASE; break;
    case 'D': case 'd': base = GPIOD_BASE; break;
    case 'E': case 'e': base = GPIOE_BASE; break;
    case 'F': case 'f': base = GPIOF_BASE; break;
    case 'G': case 'g': base = GPIOG_BASE; break;
    case 'H': case 'h': base = GPIOH_BASE; break;
    case 'I': case 'i': base = GPIOI_BASE; break;
    case 'J': case 'j': base = GPIOJ_BASE; break;
    case 'K': case 'k': base = GPIOK_BASE; break;
    default: return 0;
    }

    p = parse_uint(num);
    if (p > 15)
        return 0;

    *port_base = base;
    *pin = (uint8_t)p;
    return 1;
}

static void pin_update_last_input(uint32_t port_base, uint8_t pin)
{
    last_input_port = port_base;
    last_input_pin = pin;
    last_input_valid = 1;
}

static void print_samples_ascii(uint32_t port_base, uint8_t pin, unsigned int n)
{
    char buf[80];
    unsigned int i = 0;

    while (i < n) {
        unsigned int chunk = (n - i > sizeof(buf) - 3) ? (sizeof(buf) - 3) : (n - i);
        unsigned int j;
        for (j = 0; j < chunk; j++) {
            buf[j] = hal_gpio_read(port_base, pin) ? '1' : '0';
        }
        buf[chunk] = '\r';
        buf[chunk + 1] = '\n';
        buf[chunk + 2] = '\0';
        hal_uart_puts(buf);
        i += chunk;
    }
}

static void print_samples_raw(uint32_t port_base, uint8_t pin, unsigned int n)
{
    char buf[80];
    unsigned int i = 0;

    while (i < n) {
        unsigned int bits = (n - i > 8) ? 8 : (n - i);
        unsigned int j;
        uint8_t byte = 0;
        for (j = 0; j < bits; j++) {
            byte = (uint8_t)((byte << 1) | (hal_gpio_read(port_base, pin) ? 1U : 0U));
        }
        snprintf(buf, sizeof(buf), "%02X ", (unsigned)byte);
        hal_uart_puts(buf);
        i += bits;
    }
    hal_uart_puts("\r\n");
}

static int parse_sample_args(const char *arg, uint32_t *port_base, uint8_t *pin,
                             unsigned int *n, int *ascii)
{
    const char *p = arg;
    char pin_name[8];
    unsigned int i = 0;

    skip_spaces(&p);
    while (p[i] != '\0' && p[i] != ' ' && p[i] != '\t' && i + 1 < sizeof(pin_name)) {
        pin_name[i] = p[i];
        i++;
    }
    pin_name[i] = '\0';

    if (!parse_pin_name(pin_name, port_base, pin))
        return 0;

    p += i;
    skip_spaces(&p);
    *n = parse_uint(p);
    if (*n == 0)
        return 0;

    while (*p >= '0' && *p <= '9')
        p++;
    skip_spaces(&p);

    if (strcmp(p, "ascii") == 0)
        *ascii = 1;
    else if (strcmp(p, "raw") == 0)
        *ascii = 0;
    else
        return 0;

    return 1;
}

static void cli_pin_pullup(const char *arg)
{
    uint32_t port;
    uint8_t pin;

    if (!parse_pin_name(arg, &port, &pin)) {
        hal_uart_puts("Usage: pin pullup <PA0>\r\n");
        return;
    }
    hal_gpio_init_pullup(port, pin);
    pin_update_last_input(port, pin);
    hal_uart_puts("ok\r\n");
}

static void cli_pin_pulldown(const char *arg)
{
    uint32_t port;
    uint8_t pin;

    if (!parse_pin_name(arg, &port, &pin)) {
        hal_uart_puts("Usage: pin pulldown <PA0>\r\n");
        return;
    }
    hal_gpio_init_pulldown(port, pin);
    pin_update_last_input(port, pin);
    hal_uart_puts("ok\r\n");
}

static void cli_pin_inp(const char *arg)
{
    uint32_t port;
    uint8_t pin;

    if (!parse_pin_name(arg, &port, &pin)) {
        hal_uart_puts("Usage: pin inp <PA0>\r\n");
        return;
    }
    hal_gpio_init_input(port, pin, GPIO_PUPD_NONE);
    pin_update_last_input(port, pin);
    hal_uart_puts("ok\r\n");
}

static void cli_pin_out(const char *arg)
{
    uint32_t port;
    uint8_t pin;

    if (!parse_pin_name(arg, &port, &pin)) {
        hal_uart_puts("Usage: pin out <PA0>\r\n");
        return;
    }
    hal_gpio_init_output(port, pin, GPIO_SPEED_HIGH);
    hal_uart_puts("ok\r\n");
}

static void cli_pin_write(const char *arg)
{
    uint32_t port;
    uint8_t pin;
    const char *p = arg;
    char pin_name[8];
    unsigned int i = 0;
    unsigned int val;

    skip_spaces(&p);
    while (p[i] != '\0' && p[i] != ' ' && p[i] != '\t' && i + 1 < sizeof(pin_name)) {
        pin_name[i] = p[i];
        i++;
    }
    pin_name[i] = '\0';

    if (!parse_pin_name(pin_name, &port, &pin)) {
        hal_uart_puts("Usage: pin write <PA0> 0|1\r\n");
        return;
    }

    p += i;
    skip_spaces(&p);
    val = parse_uint(p);
    if (val > 1) {
        hal_uart_puts("Usage: pin write <PA0> 0|1\r\n");
        return;
    }
    hal_gpio_write(port, pin, (uint8_t)val);
    hal_uart_puts("ok\r\n");
}

static void cli_pin_read(const char *arg)
{
    uint32_t port;
    uint8_t pin;
    unsigned int n;
    int ascii;

    if (!parse_sample_args(arg, &port, &pin, &n, &ascii)) {
        hal_uart_puts("Usage: pin read <PA0> <n> raw|ascii\r\n");
        return;
    }

    if (ascii)
        print_samples_ascii(port, pin, n);
    else
        print_samples_raw(port, pin, n);
}

static void cli_inp_read(const char *arg)
{
    const char *p = arg;
    unsigned int n;
    int ascii = 0;

    if (!last_input_valid) {
        hal_uart_puts("inp: no input pin configured (use pin pullup/pulldown/inp first)\r\n");
        return;
    }

    skip_spaces(&p);
    n = parse_uint(p);
    if (n == 0) {
        hal_uart_puts("Usage: inp read <n> raw|ascii\r\n");
        return;
    }

    while (*p >= '0' && *p <= '9')
        p++;
    skip_spaces(&p);

    if (strcmp(p, "ascii") == 0)
        ascii = 1;
    else if (strcmp(p, "raw") == 0)
        ascii = 0;
    else {
        hal_uart_puts("Usage: inp read <n> raw|ascii\r\n");
        return;
    }

    if (ascii)
        print_samples_ascii(last_input_port, last_input_pin, n);
    else
        print_samples_raw(last_input_port, last_input_pin, n);
}

static void cli_pin(const char *arg)
{
    const char *sub;

    if (!arg || *arg == '\0') {
        hal_uart_puts("Usage: pin pullup/pulldown/inp/out/read/write\r\n");
        return;
    }

    if (strncmp(arg, "pullup", 6) == 0 && (arg[6] == '\0' || arg[6] == ' ' || arg[6] == '\t')) {
        sub = arg + 6;
        skip_spaces(&sub);
        cli_pin_pullup(sub);
        return;
    }

    if (strncmp(arg, "pulldown", 8) == 0 && (arg[8] == '\0' || arg[8] == ' ' || arg[8] == '\t')) {
        sub = arg + 8;
        skip_spaces(&sub);
        cli_pin_pulldown(sub);
        return;
    }

    if (strncmp(arg, "inp", 3) == 0 && (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
        sub = arg + 3;
        skip_spaces(&sub);
        cli_pin_inp(sub);
        return;
    }

    if (strncmp(arg, "out", 3) == 0 && (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
        sub = arg + 3;
        skip_spaces(&sub);
        cli_pin_out(sub);
        return;
    }

    if (strncmp(arg, "read", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ' || arg[4] == '\t')) {
        sub = arg + 4;
        skip_spaces(&sub);
        cli_pin_read(sub);
        return;
    }

    if (strncmp(arg, "write", 5) == 0 && (arg[5] == '\0' || arg[5] == ' ' || arg[5] == '\t')) {
        sub = arg + 5;
        skip_spaces(&sub);
        cli_pin_write(sub);
        return;
    }

    hal_uart_puts("Usage: pin pullup/pulldown/inp/out/read/write\r\n");
}

static void cli_inp(const char *arg)
{
    const char *sub;

    if (!arg || *arg == '\0') {
        hal_uart_puts("Usage: inp read <n> raw|ascii\r\n");
        return;
    }

    if (strncmp(arg, "read", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ' || arg[4] == '\t')) {
        sub = arg + 4;
        skip_spaces(&sub);
        cli_inp_read(sub);
        return;
    }

    hal_uart_puts("Usage: inp read <n> raw|ascii\r\n");
}

static void cli_sd(const char *arg)
{
    const char *sub;

    if (!arg || *arg == '\0') {
        hal_uart_puts("Usage: sd init/info/inspect/ls/cat\r\n");
        return;
    }

    if (strncmp(arg, "init", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ' || arg[4] == '\t')) {
        cli_sd_init();
        return;
    }

    if (strcmp(arg, "info") == 0) {
        cli_sd_info();
        return;
    }

    if (strcmp(arg, "inspect") == 0) {
        cli_sd_inspect();
        return;
    }

    if (strncmp(arg, "ls", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ' || arg[2] == '\t')) {
        sub = arg + 2;
        skip_spaces(&sub);
        cli_sd_ls(sub);
        return;
    }

    if (strncmp(arg, "cat", 3) == 0 && (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
        sub = arg + 3;
        skip_spaces(&sub);
        cli_sd_cat(sub);
        return;
    }

#ifdef ENABLE_TOOL_FSUTILS
    if (strncmp(arg, "rm", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ' || arg[2] == '\t')) {
        sub = arg + 2;
        skip_spaces(&sub);
        cli_sd_rm(sub);
        return;
    }

    if (strncmp(arg, "mkdir", 5) == 0 && (arg[5] == '\0' || arg[5] == ' ' || arg[5] == '\t')) {
        sub = arg + 5;
        skip_spaces(&sub);
        cli_sd_mkdir(sub);
        return;
    }

    if (strncmp(arg, "echo", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ' || arg[4] == '\t')) {
        sub = arg + 4;
        skip_spaces(&sub);
        cli_sd_echo(sub);
        return;
    }

    if (strncmp(arg, "tee", 3) == 0 && (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
        sub = arg + 3;
        skip_spaces(&sub);
        cli_sd_tee(sub);
        return;
    }

#ifdef ENABLE_TOOL_COREUTILS
    if (strncmp(arg, "touch", 5) == 0 && (arg[5] == '\0' || arg[5] == ' ' || arg[5] == '\t')) {
        sub = arg + 5;
        skip_spaces(&sub);
        cli_sd_touch(sub);
        return;
    }

    if (strncmp(arg, "cp", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ' || arg[2] == '\t')) {
        sub = arg + 2;
        skip_spaces(&sub);
        cli_sd_cp(sub);
        return;
    }

    if (strncmp(arg, "mv", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ' || arg[2] == '\t')) {
        sub = arg + 2;
        skip_spaces(&sub);
        cli_sd_mv(sub);
        return;
    }

    if (strncmp(arg, "chmod", 5) == 0 && (arg[5] == '\0' || arg[5] == ' ' || arg[5] == '\t')) {
        sub = arg + 5;
        skip_spaces(&sub);
        cli_sd_chmod(sub);
        return;
    }
#endif

    if (strcmp(arg, "mountfs") == 0) {
        cli_sd_mountfs();
        return;
    }
#ifdef ENABLE_TOOL_VI
    if (strncmp(arg, "vi", 2) == 0 && (arg[2] == '\0' || arg[2] == ' ' || arg[2] == '\t')) {
        sub = arg + 2;
        skip_spaces(&sub);
        cli_sd_vi(sub);
        return;
    }
#endif
#endif

    hal_uart_puts("Usage: sd init/info/inspect/ls/cat");
#ifdef ENABLE_TOOL_FSUTILS
    hal_uart_puts("/rm/mkdir/echo/tee");
#ifdef ENABLE_TOOL_COREUTILS
    hal_uart_puts("/touch/cp/mv/chmod");
#endif
    hal_uart_puts("/mountfs");
#ifdef ENABLE_TOOL_VI
    hal_uart_puts("/vi");
#endif
#endif
    hal_uart_puts("\r\n");
}

static void cli_net(const char *arg)
{
    const char *sub;

    if (!arg || *arg == '\0') {
        hal_uart_puts("Usage: net init/status/show, http start\r\n");
        return;
    }

    if (strcmp(arg, "init") == 0) {
        cli_net_init();
        return;
    }

    if (strcmp(arg, "status") == 0) {
        cli_net_status();
        return;
    }

    if (strncmp(arg, "show", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ' || arg[4] == '\t')) {
        sub = arg + 4;
        skip_spaces(&sub);

        if (strcmp(sub, "mac") == 0) {
            cli_net_show_mac();
            return;
        }

        if (strcmp(sub, "ipv4") == 0) {
            cli_net_show_ipv4();
            return;
        }

        if (strcmp(sub, "ipv6") == 0) {
            cli_net_show_ipv6();
            return;
        }

        hal_uart_puts("Usage: net show mac/ipv4/ipv6\r\n");
        return;
    }

    hal_uart_puts("Usage: net init/status/show, http start\r\n");
}

static void cli_unknown(const char *cmd)
{
    hal_uart_puts("Unknown command: ");
    hal_uart_puts(cmd);
    hal_uart_puts("\r\nType 'help' for available commands.\r\n");
}

/* App command registration table. Apps linked into the image can call
 * cli_register_command() from app_register_commands() to add commands.
 */
#define APP_CMD_MAX 8

typedef void (*app_cmd_fn_t)(const char *arg);

static struct {
    const char *name;
    app_cmd_fn_t fn;
} app_cmds[APP_CMD_MAX];
static int app_cmd_count = 0;

void cli_register_command(const char *name, app_cmd_fn_t fn)
{
    if (app_cmd_count >= APP_CMD_MAX || !name || !fn)
        return;
    app_cmds[app_cmd_count].name = name;
    app_cmds[app_cmd_count].fn = fn;
    app_cmd_count++;
}

__attribute__((weak)) void app_register_commands(void)
{
}

static void cli_execute(char *line)
{
    while (*line == ' ' || *line == '\t')
        line++;

    if (*line == '\0')
        return;

    char *space = strchr(line, ' ');
    char *arg = NULL;
    if (space) {
        *space = '\0';
        arg = space + 1;
        while (*arg == ' ' || *arg == '\t')
            arg++;
    }

    for (int i = 0; i < app_cmd_count; i++) {
        if (strcmp(line, app_cmds[i].name) == 0) {
            app_cmds[i].fn(arg);
            return;
        }
    }

    if (strcmp(line, "help") == 0) {
        cli_help();
    } else if (strcmp(line, "status") == 0) {
        cli_status();
    } else if (strcmp(line, "alloc") == 0) {
        cli_alloc(arg);
    } else if (strcmp(line, "reset") == 0) {
        cli_reset();
    } else if (strcmp(line, "rotate") == 0) {
        cli_rotate();
    } else if (strcmp(line, "hello") == 0) {
        cli_hello();
    } else if (strcmp(line, "heartbeat") == 0) {
        cli_heartbeat();
    } else if (strcmp(line, "program") == 0) {
        cli_program(arg);
    } else if (strcmp(line, "clear") == 0) {
        cli_clear();
    } else if (strcmp(line, "net") == 0) {
        cli_net(arg);
    } else if (strcmp(line, "http") == 0 && arg && strcmp(arg, "start") == 0) {
        cli_http_start();
    } else if (strcmp(line, "sd") == 0) {
        cli_sd(arg);
    } else if (strcmp(line, "pin") == 0) {
        cli_pin(arg);
    } else if (strcmp(line, "inp") == 0) {
        cli_inp(arg);
    } else {
        cli_unknown(line);
    }
}

/* Synchronous line reader for non-process contexts (program editor/runner). */
static void cli_read_line(char *buf, size_t size)
{
    size_t i = 0;
    int c;

    while (1) {
        c = hal_uart_try_getc();
        if (c < 0)
            continue;

        if (c == '\r' || c == '\n') {
            hal_uart_puts("\r\n");
            buf[i] = '\0';
            return;
        } else if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                hal_uart_puts("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            if (i + 1 < size) {
                buf[i++] = (char)c;
                hal_uart_putc((char)c);
            }
        }
    }
}

/* Scheduler housekeeping hook: drive lwIP and libttak background tasks. */
void saramos_sched_housekeeping(void)
{
    if (net_initialized) {
        ethernetif_input(&gnetif);
        sys_check_timeouts();
        hal_eth_poll();
    }
    ttak_cooperative_run_once(ttak_get_tick_count());
}

static int cli_input_process(saramos_process_t *p)
{
    static char cmd_buf[CMD_BUF_SIZE];
    static size_t i = 0;
    int c;

    PROC_BEGIN(p);
    while (1) {
        i = 0;
        cli_prompt();
        while (1) {
            PROC_GETC(p, c);
            if (c == '\r' || c == '\n') {
                proc_puts("\r\n");
                cmd_buf[i] = '\0';
                cli_execute(cmd_buf);
                PROC_WAIT(p, !saramos_proc_has_children(p));
                saramos_proc_wait_children(p);
                break;
            } else if (c == '\b' || c == 127) {
                if (i > 0) {
                    i--;
                    proc_puts("\b \b");
                }
            } else if (c >= 32 && c < 127) {
                if (i + 1 < sizeof(cmd_buf)) {
                    cmd_buf[i++] = (char)c;
                    proc_putc((char)c);
                }
            }
        }
    }
    PROC_END(p);
}

int main(void)
{
    hal_uart_init();

    hal_uart_puts("\r\n");
    hal_uart_puts("=== saramOS on STM32F769I-DISC1 ===\r\n");
    hal_uart_puts("Type 'help' for available commands.\r\n\r\n");

    saramos_kernel_init();
    hal_uart_puts("saramOS: resilient kernel core init OK\r\n");

    /* --- init saramOS arena --- */
    if (saramos_arena_init(&sys_arena)) {
        arena_initialized = 1;
        hal_uart_puts("saramOS: arena init OK\r\n");
    } else {
        hal_uart_puts("saramOS: arena init FAILED\r\n");
    }

    /* --- init saramOS owner --- */
    if (saramos_owner_init(&sys_owner, "kernel")) {
        owner_initialized = 1;
        hal_uart_puts("saramOS: owner init OK\r\n");
    } else {
        hal_uart_puts("saramOS: owner init FAILED\r\n");
    }

    /* --- 1 ms tick source --- */
    hal_systick_init();
    hal_uart_puts("saramOS: systick 1ms OK\r\n");

    /* --- cooperative scheduler init --- */
    ttak_async_init(0);
    saramos_sched_init();
    hal_uart_puts("libttak: async scheduler init OK\r\n");
#ifdef ENABLE_BUILTIN_EXAMPLES
    program_init_builtins();
    hal_uart_puts("example: calculator programs loaded (arith, modulo)\r\n");
#endif
    app_register_commands();
    hal_uart_puts("===================================\r\n");

    saramos_proc_spawn("cli", cli_input_process, 0, NULL, NULL, NULL, 0);
    saramos_sched_run();

    return 0;
}
