/*
 * Enhanced shell for saramOS.
 */

#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <os/saramos_process.h>

#define SHELL_MAX_ENV     16
#define SHELL_ENV_SIZE    64
#define SHELL_MAX_TOKENS  32
#define SHELL_MAX_GLOB    32
#define SHELL_LINE_SIZE   256
#define SHELL_MAX_HISTORY 8

#define SHELL_MAX_PIPE_STAGES 8
#define SHELL_MAX_STAGE_ARGS  16

/* Environment variable table. */
extern char shell_env[SHELL_MAX_ENV][SHELL_ENV_SIZE];

int shell_env_get(const char *name, char *out, size_t size);
int shell_env_set(const char *name, const char *value);
int shell_env_export(const char *name, const char *value);
int shell_env_unset(const char *name);
void shell_env_print(void);

/* Parsing helpers. */
int shell_tokenize(char *line, char *tokens[], int max_tokens);
int shell_expand(const char *src, char *dst, size_t size);
int shell_glob(const char *pattern, char matches[][SHELL_LINE_SIZE], int max_matches);

/* Shell lifecycle. */
int shell_should_exit(void);
void shell_request_exit(void);
void shell_clear_exit(void);

/* Execution. */
int shell_execute(int argc, char *argv[]);
int shell_run_file(const char *path);
int shell_run(void);
int shell_interactive_process(saramos_process_t *p);

/* Built-ins. */
int shell_builtin_cd(int argc, char *argv[]);
int shell_builtin_pwd(int argc, char *argv[]);
int shell_builtin_export(int argc, char *argv[]);
int shell_builtin_unset(int argc, char *argv[]);
int shell_builtin_env(int argc, char *argv[]);
int shell_builtin_exit(int argc, char *argv[]);
int shell_builtin_clear(int argc, char *argv[]);
int shell_builtin_test(int argc, char *argv[]);
int shell_builtin_echo(int argc, char *argv[]);
int shell_builtin_source(int argc, char *argv[]);

/* Process wrappers for pipe-aware commands. */
int proc_echo(saramos_process_t *p);
int proc_cat(saramos_process_t *p);
int proc_grep(saramos_process_t *p);
int proc_head(saramos_process_t *p);
int proc_wc(saramos_process_t *p);
int proc_sync_wrapper(saramos_process_t *p);

#endif /* SHELL_H */
