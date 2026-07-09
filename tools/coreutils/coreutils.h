/*
 * Minimal coreutils-like utilities for saramOS.
 *
 * These use FatFs directly and expose a function-call entry point so they can
 * be linked into the saramOS image and invoked from the enhanced shell.
 */

#ifndef COREUTILS_H
#define COREUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

int saramos_cp(int argc, char *argv[]);
int saramos_mv(int argc, char *argv[]);
int saramos_touch(int argc, char *argv[]);
int saramos_chmod(int argc, char *argv[]);

int saramos_head(int argc, char *argv[]);
int saramos_tail(int argc, char *argv[]);
int saramos_wc(int argc, char *argv[]);
int saramos_grep(int argc, char *argv[]);
int saramos_sort(int argc, char *argv[]);
int saramos_uniq(int argc, char *argv[]);
int saramos_diff(int argc, char *argv[]);
int saramos_cut(int argc, char *argv[]);
int saramos_tr(int argc, char *argv[]);
int saramos_rev(int argc, char *argv[]);

int saramos_find(int argc, char *argv[]);
int saramos_which(int argc, char *argv[]);
int saramos_ls(int argc, char *argv[]);

int saramos_basename(int argc, char *argv[]);
int saramos_dirname(int argc, char *argv[]);

int saramos_uptime(int argc, char *argv[]);
int saramos_date(int argc, char *argv[]);
int saramos_uname(int argc, char *argv[]);
int saramos_df(int argc, char *argv[]);
int saramos_du(int argc, char *argv[]);
int saramos_clear(int argc, char *argv[]);

int saramos_sleep(int argc, char *argv[]);
int saramos_seq(int argc, char *argv[]);
int saramos_yes(int argc, char *argv[]);
int saramos_true(int argc, char *argv[]);
int saramos_false(int argc, char *argv[]);
int saramos_env(int argc, char *argv[]);

int saramos_tac(int argc, char *argv[]);
int saramos_nl(int argc, char *argv[]);
int saramos_fold(int argc, char *argv[]);
int saramos_od(int argc, char *argv[]);
int saramos_strings(int argc, char *argv[]);
int saramos_printf(int argc, char *argv[]);
int saramos_paste(int argc, char *argv[]);
int saramos_xargs(int argc, char *argv[]);

#endif /* COREUTILS_H */
