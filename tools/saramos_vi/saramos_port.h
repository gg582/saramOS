/*
 * saramOS vi porting layer.
 *
 * Provides POSIX-like stubs that forward file operations to FatFs and
 * terminal I/O to the saramOS UART console.
 */

#ifndef SARAMOS_PORT_H
#define SARAMOS_PORT_H

#include <stddef.h>
#include <sys/types.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0x0100
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define F_OK 0
#define W_OK 2
#define R_OK 4

int saramos_open(const char *path, int flags, ...);
ssize_t saramos_read(int fd, void *buf, size_t count);
ssize_t saramos_write(int fd, const void *buf, size_t count);
int saramos_close(int fd);
off_t saramos_lseek(int fd, off_t offset, int whence);
int saramos_isatty(int fd);
int saramos_fstat(int fd, void *st);
int saramos_stat(const char *path, void *st);
int saramos_access(const char *path, int mode);
int saramos_chdir(const char *path);
char *saramos_getcwd(char *buf, size_t size);
char *saramos_getenv(const char *name);

int saramos_getchar(void);
int saramos_putchar(int c);
int saramos_puts(const char *s);

int saramos_vi(int argc, char *argv[]);

#endif /* SARAMOS_PORT_H */
