/*
 * POSIX-like file I/O stubs mapped onto FatFs.
 */

#include <string.h>
#include "ff.h"
#include "saramos_port.h"

#define SARAMOS_FD_MAX 4

struct saramos_fd {
    int used;
    FIL fil;
    off_t pos;
};

static struct saramos_fd fd_table[SARAMOS_FD_MAX];

static int alloc_fd(void)
{
    for (int i = 0; i < SARAMOS_FD_MAX; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].pos = 0;
            return i;
        }
    }
    return -1;
}

int saramos_open(const char *path, int flags, ...)
{
    BYTE mode = 0;

    if ((flags & O_RDWR) == O_RDWR)
        mode |= FA_READ | FA_WRITE;
    else if (flags & O_WRONLY)
        mode |= FA_WRITE;
    else
        mode |= FA_READ;

    if (flags & O_CREAT) {
        if (flags & O_TRUNC)
            mode |= FA_CREATE_ALWAYS;
        else
            mode |= FA_OPEN_ALWAYS;
    }

    int fd = alloc_fd();
    if (fd < 0)
        return -1;

    FRESULT res = f_open(&fd_table[fd].fil, path, mode);
    if (res != FR_OK) {
        fd_table[fd].used = 0;
        return -1;
    }

    if ((flags & O_APPEND) && (mode & FA_WRITE))
        f_lseek(&fd_table[fd].fil, f_size(&fd_table[fd].fil));

    return fd;
}

ssize_t saramos_read(int fd, void *buf, size_t count)
{
    if (fd < 0 || fd >= SARAMOS_FD_MAX || !fd_table[fd].used)
        return -1;

    UINT br;
    FRESULT res = f_read(&fd_table[fd].fil, buf, (UINT)count, &br);
    if (res != FR_OK)
        return -1;

    fd_table[fd].pos += br;
    return (ssize_t)br;
}

ssize_t saramos_write(int fd, const void *buf, size_t count)
{
    if (fd < 0 || fd >= SARAMOS_FD_MAX || !fd_table[fd].used)
        return -1;

    UINT bw;
    FRESULT res = f_write(&fd_table[fd].fil, buf, (UINT)count, &bw);
    if (res != FR_OK)
        return -1;

    fd_table[fd].pos += bw;
    return (ssize_t)bw;
}

static int saramos_fresult_to_errno(FRESULT res)
{
    return (res == FR_OK) ? 0 : -1;
}

int saramos_close(int fd)
{
    if (fd < 0 || fd >= SARAMOS_FD_MAX || !fd_table[fd].used)
        return -1;

    FRESULT res = f_close(&fd_table[fd].fil);
    fd_table[fd].used = 0;
    return saramos_fresult_to_errno(res);
}

off_t saramos_lseek(int fd, off_t offset, int whence)
{
    if (fd < 0 || fd >= SARAMOS_FD_MAX || !fd_table[fd].used)
        return -1;

    off_t newpos = 0;
    switch (whence) {
    case SEEK_SET: newpos = offset; break;
    case SEEK_CUR: newpos = fd_table[fd].pos + offset; break;
    case SEEK_END: newpos = (off_t)f_size(&fd_table[fd].fil) + offset; break;
    default: return -1;
    }

    if (newpos < 0)
        newpos = 0;

    FRESULT res = f_lseek(&fd_table[fd].fil, (FSIZE_t)newpos);
    if (res != FR_OK)
        return -1;

    fd_table[fd].pos = newpos;
    return newpos;
}

int saramos_isatty(int fd)
{
    (void)fd;
    return 0;
}

int saramos_fstat(int fd, void *st)
{
    (void)fd;
    (void)st;
    return -1;
}

int saramos_stat(const char *path, void *st)
{
    (void)path;
    (void)st;
    return -1;
}

int saramos_access(const char *path, int mode)
{
    (void)mode;
    FILINFO info;
    return (f_stat(path, &info) == FR_OK) ? 0 : -1;
}

int saramos_chdir(const char *path)
{
    return (f_chdir(path) == FR_OK) ? 0 : -1;
}

char *saramos_getcwd(char *buf, size_t size)
{
    if (f_getcwd(buf, (UINT)size) == FR_OK)
        return buf;
    return NULL;
}
