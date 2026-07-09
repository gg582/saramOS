/*
 * Minimal vi-like line editor for saramOS.
 *
 * It keeps the familiar vi flow for a serial console: use i/a/o to enter
 * insert mode, type one or more lines, press ESC or a single "." line to
 * return to command mode, then use :w, :q, or :wq. For minicom-friendly use,
 * a single ":wq" line while inserting also saves and exits.
 */

#include <stdio.h>
#include <string.h>
#include "saramos_port.h"

#define VI_MAX_LINES 64
#define VI_LINE_SIZE 128

static char vi_lines[VI_MAX_LINES][VI_LINE_SIZE];
static int vi_line_count = 0;
static int vi_cur_line = 0;

typedef enum {
    VI_INSERT_DONE = 0,
    VI_INSERT_SAVE,
    VI_INSERT_SAVE_QUIT,
} vi_insert_result_t;

static void vi_load(const char *path)
{
    int fd = saramos_open(path, O_RDONLY, 0);
    if (fd < 0)
        return;

    char c;
    int bi = 0;
    while (saramos_read(fd, &c, 1) == 1 && vi_line_count < VI_MAX_LINES) {
        if (c == '\n' || c == '\r') {
            if (c == '\r')
                continue;
            vi_lines[vi_line_count][bi] = '\0';
            vi_line_count++;
            bi = 0;
        } else if (bi < VI_LINE_SIZE - 1) {
            vi_lines[vi_line_count][bi++] = c;
        }
    }

    if (bi > 0 && vi_line_count < VI_MAX_LINES) {
        vi_lines[vi_line_count][bi] = '\0';
        vi_line_count++;
    }

    saramos_close(fd);
}

static void vi_save(const char *path)
{
    int fd = saramos_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        saramos_puts("vi: cannot write\r\n");
        return;
    }

    for (int i = 0; i < vi_line_count; i++) {
        saramos_write(fd, vi_lines[i], strlen(vi_lines[i]));
        saramos_write(fd, "\r\n", 2);
    }

    saramos_close(fd);
    saramos_puts("written\r\n");
}

static void vi_insert_line_at(const char *text, int pos)
{
    if (vi_line_count >= VI_MAX_LINES) {
        saramos_puts("vi: buffer full\r\n");
        return;
    }

    if (pos < 0) pos = 0;
    if (pos > vi_line_count) pos = vi_line_count;

    for (int j = vi_line_count; j > pos; j--)
        strcpy(vi_lines[j], vi_lines[j - 1]);

    strncpy(vi_lines[pos], text, VI_LINE_SIZE - 1);
    vi_lines[pos][VI_LINE_SIZE - 1] = '\0';
    vi_line_count++;
    vi_cur_line = pos;
}

static void vi_delete_line(void)
{
    if (vi_line_count <= 0 || vi_cur_line >= vi_line_count)
        return;

    for (int j = vi_cur_line; j < vi_line_count - 1; j++)
        strcpy(vi_lines[j], vi_lines[j + 1]);

    vi_line_count--;
    if (vi_cur_line >= vi_line_count)
        vi_cur_line = vi_line_count > 0 ? vi_line_count - 1 : 0;
}

static void vi_read_line(char *buf, size_t size)
{
    size_t i = 0;

    while (1) {
        int c = saramos_getchar();
        if (c == '\r' || c == '\n') {
            saramos_puts("\r\n");
            buf[i] = '\0';
            return;
        } else if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                saramos_puts("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            if (i + 1 < size) {
                buf[i++] = (char)c;
                saramos_putchar(c);
            }
        }
    }
}

static vi_insert_result_t vi_insert_mode(int start_pos)
{
    char line[VI_LINE_SIZE];
    size_t i = 0;
    int pos = start_pos;

    saramos_puts("-- INSERT --\r\n");

    while (1) {
        int c = saramos_getchar();

        if (c == 27) {
            saramos_puts("\r\n");
            if (i > 0) {
                line[i] = '\0';
                vi_insert_line_at(line, pos++);
            }
            saramos_puts("-- COMMAND --\r\n");
            return VI_INSERT_DONE;
        }

        if (c == '\r' || c == '\n') {
            saramos_puts("\r\n");
            line[i] = '\0';

            if (strcmp(line, ".") == 0) {
                saramos_puts("-- COMMAND --\r\n");
                return VI_INSERT_DONE;
            }
            if (strcmp(line, ":w") == 0)
                return VI_INSERT_SAVE;
            if (strcmp(line, ":wq") == 0)
                return VI_INSERT_SAVE_QUIT;

            vi_insert_line_at(line, pos++);
            i = 0;
            continue;
        }

        if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                saramos_puts("\b \b");
            }
            continue;
        }

        if (c >= 32 && c < 127 && i + 1 < sizeof(line)) {
            line[i++] = (char)c;
            saramos_putchar(c);
        }
    }
}

int saramos_vi(int argc, char *argv[])
{
    if (argc < 2) {
        saramos_puts("vi: missing file operand\r\n");
        return 1;
    }

    const char *path = argv[1];
    memset(vi_lines, 0, sizeof(vi_lines));
    vi_line_count = 0;
    vi_cur_line = 0;
    vi_load(path);

    int running = 1;
    while (running) {
        char prompt[32];
        snprintf(prompt, sizeof(prompt), "[%d/%d] ", vi_line_count ? vi_cur_line + 1 : 0, vi_line_count);
        saramos_puts(prompt);

        char cmd[VI_LINE_SIZE];
        vi_read_line(cmd, sizeof(cmd));

        if (cmd[0] == '\0')
            continue;

        if (cmd[0] == 'i' && cmd[1] == '\0') {
            vi_insert_result_t r = vi_insert_mode(vi_cur_line);
            if (r == VI_INSERT_SAVE || r == VI_INSERT_SAVE_QUIT)
                vi_save(path);
            if (r == VI_INSERT_SAVE_QUIT)
                running = 0;
        } else if (cmd[0] == 'a' && cmd[1] == '\0') {
            vi_insert_result_t r = vi_insert_mode(vi_cur_line + 1);
            if (r == VI_INSERT_SAVE || r == VI_INSERT_SAVE_QUIT)
                vi_save(path);
            if (r == VI_INSERT_SAVE_QUIT)
                running = 0;
        } else if (cmd[0] == 'o' && cmd[1] == '\0') {
            vi_insert_result_t r = vi_insert_mode(vi_cur_line + 1);
            if (r == VI_INSERT_SAVE || r == VI_INSERT_SAVE_QUIT)
                vi_save(path);
            if (r == VI_INSERT_SAVE_QUIT)
                running = 0;
        } else if ((cmd[0] == 'd' && cmd[1] == '\0') ||
                   (cmd[0] == 'x' && cmd[1] == '\0')) {
            vi_delete_line();
        } else if (cmd[0] == 'n' || cmd[0] == 'j' || cmd[0] == 'l') {
            if (vi_cur_line + 1 < vi_line_count)
                vi_cur_line++;
        } else if (cmd[0] == 'p' || cmd[0] == 'k' || cmd[0] == 'h') {
            if (vi_cur_line > 0)
                vi_cur_line--;
        } else if (strcmp(cmd, ":w") == 0) {
            vi_save(path);
        } else if (strcmp(cmd, ":q") == 0) {
            running = 0;
        } else if (strcmp(cmd, ":wq") == 0) {
            vi_save(path);
            running = 0;
        } else {
            saramos_puts("vi: use i/a/o to insert, ESC or . to stop inserting, :wq to save\r\n");
        }
    }

    return 0;
}
