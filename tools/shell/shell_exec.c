/*
 * Command dispatcher and simple control flow for the enhanced saramOS shell.
 */

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <os/saramos_pipe.h>
#include "shell.h"
#include "ff.h"

#ifdef ENABLE_TOOL_FSUTILS
#include "fsutils.h"
#endif
#ifdef ENABLE_TOOL_VI
#include "saramos_port.h"
#endif
#ifdef ENABLE_TOOL_COREUTILS
#include "coreutils.h"
#endif

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);
extern int hal_uart_try_getc(void);

/* Command-to-process mapping for pipe stages. */
typedef struct {
    const char *name;
    saramos_proc_fn_t fn;
} shell_proc_map_t;

static const shell_proc_map_t shell_proc_map[] = {
    {"echo", proc_echo},
    {"cat",  proc_cat},
    {"grep", proc_grep},
    {"head", proc_head},
    {"wc",   proc_wc},
    {NULL, NULL}
};

static saramos_proc_fn_t shell_lookup_proc(const char *name)
{
    for (const shell_proc_map_t *m = shell_proc_map; m->name != NULL; m++) {
        if (strcmp(name, m->name) == 0)
            return m->fn;
    }
    return NULL;
}

static int dispatch_external(int argc, char *argv[])
{
#ifdef ENABLE_TOOL_COREUTILS
    if (strcmp(argv[0], "ls") == 0)     return saramos_ls(argc, argv);
    if (strcmp(argv[0], "cp") == 0)     return saramos_cp(argc, argv);
    if (strcmp(argv[0], "mv") == 0)     return saramos_mv(argc, argv);
    if (strcmp(argv[0], "touch") == 0)  return saramos_touch(argc, argv);
    if (strcmp(argv[0], "chmod") == 0)  return saramos_chmod(argc, argv);
    if (strcmp(argv[0], "head") == 0)   return saramos_head(argc, argv);
    if (strcmp(argv[0], "tail") == 0)   return saramos_tail(argc, argv);
    if (strcmp(argv[0], "wc") == 0)     return saramos_wc(argc, argv);
    if (strcmp(argv[0], "grep") == 0)   return saramos_grep(argc, argv);
    if (strcmp(argv[0], "sort") == 0)   return saramos_sort(argc, argv);
    if (strcmp(argv[0], "uniq") == 0)   return saramos_uniq(argc, argv);
    if (strcmp(argv[0], "diff") == 0)   return saramos_diff(argc, argv);
    if (strcmp(argv[0], "cut") == 0)    return saramos_cut(argc, argv);
    if (strcmp(argv[0], "tr") == 0)     return saramos_tr(argc, argv);
    if (strcmp(argv[0], "rev") == 0)    return saramos_rev(argc, argv);
    if (strcmp(argv[0], "find") == 0)   return saramos_find(argc, argv);
    if (strcmp(argv[0], "which") == 0)  return saramos_which(argc, argv);
    if (strcmp(argv[0], "basename") == 0) return saramos_basename(argc, argv);
    if (strcmp(argv[0], "dirname") == 0)  return saramos_dirname(argc, argv);
    if (strcmp(argv[0], "uptime") == 0) return saramos_uptime(argc, argv);
    if (strcmp(argv[0], "date") == 0)   return saramos_date(argc, argv);
    if (strcmp(argv[0], "uname") == 0)  return saramos_uname(argc, argv);
    if (strcmp(argv[0], "df") == 0)     return saramos_df(argc, argv);
    if (strcmp(argv[0], "du") == 0)     return saramos_du(argc, argv);
    if (strcmp(argv[0], "clear") == 0)  return saramos_clear(argc, argv);
    if (strcmp(argv[0], "sleep") == 0)  return saramos_sleep(argc, argv);
    if (strcmp(argv[0], "seq") == 0)    return saramos_seq(argc, argv);
    if (strcmp(argv[0], "yes") == 0)    return saramos_yes(argc, argv);
    if (strcmp(argv[0], "true") == 0)   return saramos_true(argc, argv);
    if (strcmp(argv[0], "false") == 0)  return saramos_false(argc, argv);
    if (strcmp(argv[0], "tac") == 0)     return saramos_tac(argc, argv);
    if (strcmp(argv[0], "nl") == 0)      return saramos_nl(argc, argv);
    if (strcmp(argv[0], "fold") == 0)    return saramos_fold(argc, argv);
    if (strcmp(argv[0], "od") == 0)      return saramos_od(argc, argv);
    if (strcmp(argv[0], "strings") == 0) return saramos_strings(argc, argv);
    if (strcmp(argv[0], "printf") == 0)  return saramos_printf(argc, argv);
    if (strcmp(argv[0], "paste") == 0)   return saramos_paste(argc, argv);
    if (strcmp(argv[0], "xargs") == 0)   return saramos_xargs(argc, argv);
#endif

#ifdef ENABLE_TOOL_FSUTILS
    if (strcmp(argv[0], "cat") == 0)    return saramos_cat(argc, argv);
    if (strcmp(argv[0], "rm") == 0)     return saramos_rm(argc, argv);
    if (strcmp(argv[0], "mkdir") == 0)  return saramos_mkdir(argc, argv);
    if (strcmp(argv[0], "tee") == 0)    return saramos_tee(argc, argv);
#endif

#ifdef ENABLE_TOOL_VI
    if (strcmp(argv[0], "vi") == 0)     return saramos_vi(argc, argv);
#endif

    return -1;
}

static int dispatch_builtin(int argc, char *argv[])
{
    if (strcmp(argv[0], "cd") == 0)     return shell_builtin_cd(argc, argv);
    if (strcmp(argv[0], "pwd") == 0)    return shell_builtin_pwd(argc, argv);
    if (strcmp(argv[0], "export") == 0) return shell_builtin_export(argc, argv);
    if (strcmp(argv[0], "unset") == 0)  return shell_builtin_unset(argc, argv);
    if (strcmp(argv[0], "env") == 0)    return shell_builtin_env(argc, argv);
    if (strcmp(argv[0], "exit") == 0)   return shell_builtin_exit(argc, argv);
    if (strcmp(argv[0], "clear") == 0)  return shell_builtin_clear(argc, argv);
    if (strcmp(argv[0], "test") == 0 || strcmp(argv[0], "[") == 0)
                                        return shell_builtin_test(argc, argv);
    if (strcmp(argv[0], "echo") == 0)   return shell_builtin_echo(argc, argv);
    if (strcmp(argv[0], "source") == 0 || strcmp(argv[0], ".") == 0)
                                        return shell_builtin_source(argc, argv);
    return -1;
}

static int execute_tokens(int argc, char *argv[])
{
    int r;

    r = dispatch_builtin(argc, argv);
    if (r >= 0)
        return r;

    r = dispatch_external(argc, argv);
    if (r >= 0)
        return r;

    hal_uart_puts("sh: command not found: ");
    hal_uart_puts(argv[0]);
    hal_uart_puts("\r\n");
    return 1;
}

static int is_assignment(const char *s)
{
    const char *eq = strchr(s, '=');
    if (!eq || eq == s)
        return 0;
    for (const char *p = s; p < eq; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    }
    return 1;
}

static int handle_for(char *tokens[], int count)
{
    /* for var in a b c; do cmd ...; done */
    int in_idx = -1, do_idx = -1, done_idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(tokens[i], "in") == 0)   in_idx = i;
        if (strcmp(tokens[i], "do") == 0)   do_idx = i;
        if (strcmp(tokens[i], "done") == 0) done_idx = i;
    }

    if (in_idx <= 1 || do_idx <= in_idx || done_idx <= do_idx) {
        hal_uart_puts("sh: syntax error in for\r\n");
        return 1;
    }

    const char *var = tokens[1];

    for (int i = in_idx + 1; i < do_idx; i++) {
        shell_env_set(var, tokens[i]);

        char expanded[SHELL_MAX_TOKENS][SHELL_LINE_SIZE];
        char *body_argv[SHELL_MAX_TOKENS];
        int body_count = 0;
        for (int j = do_idx + 1; j < done_idx && body_count < SHELL_MAX_TOKENS; j++) {
            shell_expand(tokens[j], expanded[body_count], sizeof(expanded[body_count]));
            body_argv[body_count] = expanded[body_count];
            body_count++;
        }

        if (body_count > 0)
            shell_execute(body_count, body_argv);
    }

    return 0;
}

static int handle_if(char *tokens[], int count)
{
    /* if [ expr ]; then cmd ...; fi */
    int then_idx = -1, fi_idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(tokens[i], "then") == 0) then_idx = i;
        if (strcmp(tokens[i], "fi") == 0)   fi_idx = i;
    }

    if (then_idx < 2 || fi_idx < then_idx) {
        hal_uart_puts("sh: syntax error in if\r\n");
        return 1;
    }

    int test_result = shell_builtin_test(then_idx, tokens);
    if (test_result == 0) {
        char expanded[SHELL_MAX_TOKENS][SHELL_LINE_SIZE];
        char *body_argv[SHELL_MAX_TOKENS];
        int body_count = 0;
        for (int j = then_idx + 1; j < fi_idx && body_count < SHELL_MAX_TOKENS; j++) {
            shell_expand(tokens[j], expanded[body_count], sizeof(expanded[body_count]));
            body_argv[body_count] = expanded[body_count];
            body_count++;
        }
        if (body_count > 0)
            shell_execute(body_count, body_argv);
    }

    return 0;
}

int shell_execute(int argc, char *argv[])
{
    saramos_process_t *p = saramos_current_proc;

    if (argc == 0)
        return 0;

    if (strcmp(argv[0], "for") == 0)
        return handle_for(argv, argc);

    if (strcmp(argv[0], "if") == 0)
        return handle_if(argv, argc);

    if (is_assignment(argv[0])) {
        char *eq = strchr(argv[0], '=');
        *eq = '\0';
        shell_env_set(argv[0], eq + 1);
        return 0;
    }

    /* Expand arguments. */
    static char expanded[SHELL_MAX_TOKENS][SHELL_LINE_SIZE];
    static char *eargv[SHELL_MAX_TOKENS];
    int eargc = 0;

    for (int i = 0; i < argc && eargc < SHELL_MAX_TOKENS; i++) {
        shell_expand(argv[i], expanded[eargc], sizeof(expanded[eargc]));

        /* If expanded contains glob metacharacters, try glob. */
        const char *pp = expanded[eargc];
        int has_glob = 0;
        while (*pp) {
            if (*pp == '*' || *pp == '?' || *pp == '[') {
                has_glob = 1;
                break;
            }
            pp++;
        }

        if (has_glob) {
            char matches[SHELL_MAX_GLOB][SHELL_LINE_SIZE];
            int n = shell_glob(expanded[eargc], matches, SHELL_MAX_GLOB);
            if (n > 0) {
                for (int j = 0; j < n && eargc < SHELL_MAX_TOKENS; j++) {
                    strncpy(expanded[eargc], matches[j], SHELL_LINE_SIZE - 1);
                    expanded[eargc][SHELL_LINE_SIZE - 1] = '\0';
                    eargv[eargc] = expanded[eargc];
                    eargc++;
                }
                continue;
            }
        }

        eargv[eargc] = expanded[eargc];
        eargc++;
    }

    /* Look for a pipe token. */
    int pipe_idx = -1;
    for (int i = 0; i < eargc; i++) {
        if (strcmp(eargv[i], "|") == 0) {
            pipe_idx = i;
            break;
        }
    }

    if (pipe_idx < 0) {
        return execute_tokens(eargc, eargv);
    }

    /* Parse pipeline stages. */
    static char stage_args[SHELL_MAX_PIPE_STAGES][SHELL_MAX_STAGE_ARGS][SHELL_LINE_SIZE];
    static char *stage_argv[SHELL_MAX_PIPE_STAGES][SHELL_MAX_STAGE_ARGS];
    static int stage_argc[SHELL_MAX_PIPE_STAGES];
    static saramos_pipe_t stage_pipes[SHELL_MAX_PIPE_STAGES - 1];

    int num_stages = 0;
    int stage_start = 0;

    for (int i = 0; i <= eargc; i++) {
        if (i == eargc || strcmp(eargv[i], "|") == 0) {
            if (i == stage_start) {
                if (p)
                    proc_puts("sh: syntax error near |\r\n");
                return 1;
            }
            if (num_stages >= SHELL_MAX_PIPE_STAGES) {
                if (p)
                    proc_puts("sh: too many pipe stages\r\n");
                return 1;
            }
            stage_argc[num_stages] = 0;
            for (int j = stage_start; j < i && stage_argc[num_stages] < SHELL_MAX_STAGE_ARGS; j++) {
                strncpy(stage_args[num_stages][stage_argc[num_stages]], eargv[j], SHELL_LINE_SIZE - 1);
                stage_args[num_stages][stage_argc[num_stages]][SHELL_LINE_SIZE - 1] = '\0';
                stage_argv[num_stages][stage_argc[num_stages]] = stage_args[num_stages][stage_argc[num_stages]];
                stage_argc[num_stages]++;
            }
            num_stages++;
            stage_start = i + 1;
        }
    }

    /* Create inter-stage pipes and spawn each stage. */
    for (int i = 0; i < num_stages - 1; i++) {
        saramos_pipe_init(&stage_pipes[i]);
    }

    for (int i = 0; i < num_stages; i++) {
        saramos_pipe_t *in_pipe  = (i == 0) ? NULL : &stage_pipes[i - 1];
        saramos_pipe_t *out_pipe = (i == num_stages - 1) ? NULL : &stage_pipes[i];
        saramos_proc_fn_t fn = shell_lookup_proc(stage_argv[i][0]);
        if (!fn)
            fn = proc_sync_wrapper;

        if (!saramos_proc_spawn_child(p, stage_argv[i][0], fn,
                                      stage_argc[i], stage_argv[i],
                                      in_pipe, out_pipe, 0)) {
            if (p)
                proc_puts("sh: failed to spawn process\r\n");
            return 1;
        }
    }

    return 0;
}

int shell_run_file(const char *path)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("source: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return 1;
    }

    char line[SHELL_LINE_SIZE];
    size_t li = 0;
    UINT br;

    while (f_read(&fil, &line[li], 1, &br) == FR_OK && br == 1) {
        if (line[li] == '\n' || li >= sizeof(line) - 2) {
            line[li] = '\0';
            char *tokens[SHELL_MAX_TOKENS];
            int count = shell_tokenize(line, tokens, SHELL_MAX_TOKENS);
            if (count > 0)
                shell_execute(count, tokens);
            li = 0;
        } else {
            li++;
        }
    }

    f_close(&fil);
    return 0;
}
