#include "cli.h"
#include "fmt.h"
#include "uart.h"

#define LINE_MAX 64u
#define WORDS_MAX 4                    /* command name + up to 3 arguments */

static char g_line[LINE_MAX];
static uint32_t g_len;

static const cli_cmd_t *g_cmds;        /* the program's own commands */
static uint32_t g_count;

static void cmd_help(char *args[]);
static void cmd_peek(char *args[]);
static void cmd_poke(char *args[]);

static const cli_cmd_t g_builtin[] = {
    { "help", 0u, "help",                  cmd_help },
    { "peek", 1u, "peek <addr>",           cmd_peek },
    { "poke", 2u, "poke <addr> <val>",     cmd_poke },
};
#define BUILTIN_COUNT (sizeof g_builtin / sizeof g_builtin[0])

static void print_prompt(void)
{
    uart_puts("> ");
}

static void print_usage_lines(const cli_cmd_t *cmds, uint32_t count)
{
    for (uint32_t i = 0u; i < count; i++) {
        uart_puts("  ");
        uart_puts(cmds[i].usage);
        uart_puts("\r\n");
    }
}

static void cmd_help(char *args[])
{
    (void)args;
    print_usage_lines(g_cmds, g_count);
    print_usage_lines(g_builtin, BUILTIN_COUNT);
}

static void cmd_peek(char *args[])
{
    uint32_t addr;

    if (!parse_u32(args[0], &addr) || (addr & 3u) != 0u) {
        uart_puts("usage: peek <4-byte-aligned addr>\r\n");
        return;
    }

    /* Print the address before reading. If the read faults, the line still
       shows which address caused it. */
    print_hex32(uart_putc, addr);
    uart_puts(" = ");
    print_hex32(uart_putc, *(volatile uint32_t *)addr);
    uart_puts("\r\n");
}

static void cmd_poke(char *args[])
{
    uint32_t addr;
    uint32_t val;

    if (!parse_u32(args[0], &addr) || (addr & 3u) != 0u ||
        !parse_u32(args[1], &val)) {
        uart_puts("usage: poke <4-byte-aligned addr> <val>\r\n");
        return;
    }

    *(volatile uint32_t *)addr = val;
    print_hex32(uart_putc, addr);
    uart_puts(" <- ");
    print_hex32(uart_putc, val);
    uart_puts("\r\n");
}

/* Returns the command named name, or 0. */
static const cli_cmd_t *find(const cli_cmd_t *cmds, uint32_t count, const char *name)
{
    for (uint32_t i = 0u; i < count; i++) {
        if (streq(cmds[i].name, name)) {
            return &cmds[i];
        }
    }
    return 0;
}

static void execute(char *line)
{
    char *words[WORDS_MAX + 1];        /* +1 so extra words can be detected */
    int n = tokenize(line, words, WORDS_MAX + 1);

    if (n == 0) {
        return;
    }

    /* The program's commands come first, so a program can override a built-in. */
    const cli_cmd_t *cmd = find(g_cmds, g_count, words[0]);
    if (cmd == 0) {
        cmd = find(g_builtin, BUILTIN_COUNT, words[0]);
    }

    if (cmd == 0) {
        uart_puts("unknown command, try 'help'\r\n");
    } else if (n - 1 != (int)cmd->nargs) {
        uart_puts("usage: ");
        uart_puts(cmd->usage);
        uart_puts("\r\n");
    } else {
        cmd->run(&words[1]);
    }
}

void cli_start(const cli_cmd_t *cmds, uint32_t count)
{
    g_cmds = cmds;
    g_count = count;

    /* A command needing more words than the tokenizer keeps could never match. */
    for (uint32_t i = 0u; i < count; i++) {
        if (cmds[i].nargs > WORDS_MAX - 1u) {
            uart_puts("cli: too many args for ");
            uart_puts(cmds[i].name);
            uart_puts("\r\n");
        }
    }

    uart_puts("\r\nSTM32F446 CLI ready. Type 'help'.\r\n");
    print_prompt();
}

void cli_feed(char c)
{
    if (c == '\r' || c == '\n') {
        uart_puts("\r\n");
        g_line[g_len] = '\0';
        execute(g_line);
        g_len = 0u;
        print_prompt();
    } else if (c == '\b' || c == 0x7F) {
        if (g_len != 0u) {
            g_len--;
            uart_puts("\b \b");
        }
    } else if (c >= 0x20 && c < 0x7F) {
        if (g_len < LINE_MAX - 1u) {
            g_line[g_len++] = c;
            uart_putc(c);
        }
    }
}
