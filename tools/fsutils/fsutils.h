/*
 * Minimal filesystem utilities for saramOS.
 *
 * These use FatFs directly and expose a function-call entry point so they can
 * be linked into the saramOS image and invoked from the CLI or from the
 * mountfs shell.
 */

#ifndef FSUTILS_H
#define FSUTILS_H

int saramos_rm(int argc, char *argv[]);
int saramos_mkdir(int argc, char *argv[]);
int saramos_cat(int argc, char *argv[]);
int saramos_echo(int argc, char *argv[]);
int saramos_tee(int argc, char *argv[]);

#endif /* FSUTILS_H */
