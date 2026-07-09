/*
 * Environment variable table for the enhanced saramOS shell.
 */

#include <string.h>
#include <stdio.h>
#include "shell.h"

extern void hal_uart_puts(const char *s);

char shell_env[SHELL_MAX_ENV][SHELL_ENV_SIZE];

static int find_slot(const char *name)
{
    size_t nlen = strlen(name);
    for (int i = 0; i < SHELL_MAX_ENV; i++) {
        const char *eq = strchr(shell_env[i], '=');
        if (eq) {
            size_t klen = (size_t)(eq - shell_env[i]);
            if (klen == nlen && strncmp(shell_env[i], name, nlen) == 0)
                return i;
        }
    }
    return -1;
}

static int empty_slot(void)
{
    for (int i = 0; i < SHELL_MAX_ENV; i++) {
        if (shell_env[i][0] == '\0')
            return i;
    }
    return -1;
}

int shell_env_get(const char *name, char *out, size_t size)
{
    int idx = find_slot(name);
    if (idx < 0)
        return 0;
    const char *val = strchr(shell_env[idx], '=');
    if (!val)
        return 0;
    val++;
    strncpy(out, val, size - 1);
    out[size - 1] = '\0';
    return 1;
}

int shell_env_set(const char *name, const char *value)
{
    if (!name || !*name)
        return 0;
    if (strlen(name) + strlen(value) + 2 > SHELL_ENV_SIZE)
        return 0;

    int idx = find_slot(name);
    if (idx < 0)
        idx = empty_slot();
    if (idx < 0)
        return 0;

    snprintf(shell_env[idx], SHELL_ENV_SIZE, "%s=%s", name, value);
    return 1;
}

int shell_env_export(const char *name, const char *value)
{
    return shell_env_set(name, value);
}

int shell_env_unset(const char *name)
{
    int idx = find_slot(name);
    if (idx < 0)
        return 0;
    shell_env[idx][0] = '\0';
    return 1;
}

void shell_env_print(void)
{
    for (int i = 0; i < SHELL_MAX_ENV; i++) {
        if (shell_env[i][0] != '\0') {
            hal_uart_puts(shell_env[i]);
            hal_uart_puts("\r\n");
        }
    }
}

void shell_env_init(void)
{
    for (int i = 0; i < SHELL_MAX_ENV; i++)
        shell_env[i][0] = '\0';
    shell_env_set("PATH", "/bin:/usr/bin");
    shell_env_set("HOME", "/");
    shell_env_set("TERM", "vt100");
}
