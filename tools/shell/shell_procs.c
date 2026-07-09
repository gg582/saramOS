/*
 * Process-function wrappers for pipe-aware shell commands.
 *
 * These run as saramOS processes under the cooperative scheduler.  They read
 * from p->stdin_pipe and write to p->stdout_pipe when present, falling back to
 * the UART otherwise.
 */

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <os/saramos_pipe.h>
#include "shell.h"
#include "ff.h"

int proc_echo(saramos_process_t *p)
{
    static int i;
    static int nflag;

    PROC_BEGIN(p);
    nflag = 0;
    i = 1;
    if (p->argc > 1 && strcmp(p->argv[1], "-n") == 0) {
        nflag = 1;
        i = 2;
    }
    for (; i < p->argc; i++) {
        PROC_PUTS(p, p->argv[i]);
        if (i + 1 < p->argc)
            PROC_PUTS(p, " ");
    }
    if (!nflag)
        PROC_PUTS(p, "\r\n");
    PROC_END(p);
}

int proc_cat(saramos_process_t *p)
{
    static int i;
    static FIL fil;
    static FRESULT res;
    static char buf[64];
    static UINT br;
    static char line[256];
    static size_t len;

    PROC_BEGIN(p);
    if (p->argc < 2) {
        if (!p->stdin_pipe) {
            PROC_PUT_LINE(p, "cat: missing file");
            PROC_EXIT(p, 1);
        }
        while (1) {
            PROC_PIPE_READ_LINE(p, p->stdin_pipe, line, sizeof(line), &len);
            if (len == 0 && saramos_pipe_eof(p->stdin_pipe))
                break;
            if (p->stdout_pipe) {
                PROC_PIPE_WRITE(p, p->stdout_pipe, line, len);
            } else {
                line[len] = '\0';
                PROC_PUTS(p, line);
            }
        }
    } else {
        for (i = 1; i < p->argc; i++) {
            res = f_open(&fil, p->argv[i], FA_READ);
            if (res != FR_OK) {
                PROC_PUTS(p, "cat: \"");
                PROC_PUTS(p, p->argv[i]);
                PROC_PUT_LINE(p, "\": No such file or directory");
                continue;
            }
            while ((res = f_read(&fil, buf, sizeof(buf) - 1, &br)) == FR_OK && br > 0) {
                if (p->stdout_pipe) {
                    PROC_PIPE_WRITE(p, p->stdout_pipe, buf, br);
                } else {
                    buf[br] = '\0';
                    PROC_PUTS(p, buf);
                }
            }
            f_close(&fil);
        }
    }
    PROC_END(p);
}

static int match(const char *line, const char *pat, int ignore_case)
{
    size_t plen = strlen(pat);
    if (plen == 0)
        return 1;
    for (const char *pp = line; *pp; pp++) {
        if (ignore_case) {
            size_t j;
            for (j = 0; j < plen; j++) {
                if (tolower((unsigned char)pp[j]) != tolower((unsigned char)pat[j]))
                    break;
            }
            if (j == plen)
                return 1;
        } else {
            if (strncmp(pp, pat, plen) == 0)
                return 1;
        }
    }
    return 0;
}

int proc_grep(saramos_process_t *p)
{
    static int ignore_case, show_line, pat_idx, file_start, i;
    static const char *pat;
    static char line[256];
    static size_t len;
    static unsigned int line_no;
    static FIL fil;
    static FRESULT res;
    static size_t li;
    static UINT br;
    static char prefix[32];

    PROC_BEGIN(p);
    ignore_case = 0;
    show_line = 0;
    pat_idx = 1;

    while (pat_idx < p->argc && p->argv[pat_idx][0] == '-') {
        if (strcmp(p->argv[pat_idx], "-i") == 0)
            ignore_case = 1;
        else if (strcmp(p->argv[pat_idx], "-n") == 0)
            show_line = 1;
        pat_idx++;
    }

    if (pat_idx >= p->argc) {
        PROC_PUT_LINE(p, "grep: missing pattern");
        PROC_EXIT(p, 1);
    }

    pat = p->argv[pat_idx];
    file_start = pat_idx + 1;

    if (file_start >= p->argc) {
        if (!p->stdin_pipe) {
            PROC_PUT_LINE(p, "grep: missing file");
            PROC_EXIT(p, 1);
        }
        line_no = 0;
        while (1) {
            PROC_PIPE_READ_LINE(p, p->stdin_pipe, line, sizeof(line), &len);
            if (len == 0 && saramos_pipe_eof(p->stdin_pipe))
                break;
            line_no++;
            if (match(line, pat, ignore_case)) {
                if (show_line) {
                    snprintf(prefix, sizeof(prefix), "%u:", line_no);
                    PROC_PUTS(p, prefix);
                }
                PROC_PUTS(p, line);
                PROC_PUTS(p, "\r\n");
            }
        }
    } else {
        for (i = file_start; i < p->argc; i++) {
            res = f_open(&fil, p->argv[i], FA_READ);
            if (res != FR_OK) {
                PROC_PUTS(p, "grep: cannot open '\"");
                PROC_PUTS(p, p->argv[i]);
                PROC_PUT_LINE(p, "\"");
                continue;
            }

            li = 0;
            line_no = 0;
            while (f_read(&fil, &line[li], 1, &br) == FR_OK && br == 1) {
                if (line[li] == '\n' || li >= sizeof(line) - 2) {
                    line[li] = '\0';
                    line_no++;
                    if (match(line, pat, ignore_case)) {
                        if (show_line) {
                            snprintf(prefix, sizeof(prefix), "%u:", line_no);
                            PROC_PUTS(p, prefix);
                        }
                        PROC_PUTS(p, line);
                        PROC_PUTS(p, "\r\n");
                    }
                    li = 0;
                } else {
                    li++;
                }
            }

            if (li > 0) {
                line[li] = '\0';
                line_no++;
                if (match(line, pat, ignore_case)) {
                    if (show_line) {
                        snprintf(prefix, sizeof(prefix), "%u:", line_no);
                        PROC_PUTS(p, prefix);
                    }
                    PROC_PUTS(p, line);
                    PROC_PUTS(p, "\r\n");
                }
            }

            f_close(&fil);
        }
    }
    PROC_END(p);
}

static unsigned int parse_uint(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (unsigned int)(*s++ - '0');
    return n;
}

int proc_head(saramos_process_t *p)
{
    static unsigned int n;
    static int file_start, i;
    static FIL fil;
    static FRESULT res;
    static unsigned int count;
    static char c;
    static UINT br;
    static char line[256];
    static size_t len;

    PROC_BEGIN(p);
    n = 10;
    file_start = 1;

    if (p->argc > 2 && strcmp(p->argv[1], "-n") == 0) {
        n = parse_uint(p->argv[2]);
        file_start = 3;
    } else if (p->argc > 1 && p->argv[1][0] == '-' && p->argv[1][1] == 'n') {
        n = parse_uint(p->argv[1] + 2);
        file_start = 2;
    }

    if (p->argc <= file_start) {
        if (!p->stdin_pipe) {
            PROC_PUT_LINE(p, "head: missing file");
            PROC_EXIT(p, 1);
        }
        count = 0;
        while (count < n) {
            PROC_PIPE_READ_LINE(p, p->stdin_pipe, line, sizeof(line), &len);
            if (len == 0 && saramos_pipe_eof(p->stdin_pipe))
                break;
            if (p->stdout_pipe) {
                PROC_PIPE_WRITE(p, p->stdout_pipe, line, len);
            } else {
                line[len] = '\0';
                PROC_PUTS(p, line);
            }
            count++;
        }
    } else {
        for (i = file_start; i < p->argc; i++) {
            res = f_open(&fil, p->argv[i], FA_READ);
            if (res != FR_OK) {
                PROC_PUTS(p, "head: cannot open '\"");
                PROC_PUTS(p, p->argv[i]);
                PROC_PUT_LINE(p, "\"");
                continue;
            }
            count = 0;
            while (count < n && f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
                PROC_PUTC(p, c);
                if (c == '\n')
                    count++;
            }
            f_close(&fil);
        }
    }
    PROC_END(p);
}

int proc_wc(saramos_process_t *p)
{
    static int show_lines, show_words, show_bytes, file_start, i;
    static unsigned long lines, words, bytes;
    static unsigned long total_lines, total_words, total_bytes;
    static int in_word;
    static FIL fil;
    static FRESULT res;
    static char buf[64];
    static UINT br;
    static size_t len;
    static char out[64];
    static size_t j;

    PROC_BEGIN(p);
    show_lines = 1;
    show_words = 1;
    show_bytes = 1;
    file_start = 1;

    for (i = 1; i < p->argc && p->argv[i][0] == '-'; i++) {
        if (strcmp(p->argv[i], "-l") == 0) {
            show_words = 0;
            show_bytes = 0;
            file_start = i + 1;
        } else if (strcmp(p->argv[i], "-w") == 0) {
            show_lines = 0;
            show_bytes = 0;
            file_start = i + 1;
        } else if (strcmp(p->argv[i], "-c") == 0) {
            show_lines = 0;
            show_words = 0;
            file_start = i + 1;
        }
    }

    if (file_start >= p->argc) {
        if (!p->stdin_pipe) {
            PROC_PUT_LINE(p, "wc: missing file");
            PROC_EXIT(p, 1);
        }
        lines = 0;
        words = 0;
        bytes = 0;
        in_word = 0;

        while (1) {
            len = 0;
            PROC_PIPE_READ(p, p->stdin_pipe, buf, sizeof(buf), &len);
            if (len == 0 && saramos_pipe_eof(p->stdin_pipe))
                break;
            for (j = 0; j < len; j++) {
                unsigned char c = (unsigned char)buf[j];
                bytes++;
                if (c == '\n')
                    lines++;
                if (isspace(c)) {
                    in_word = 0;
                } else if (!in_word) {
                    in_word = 1;
                    words++;
                }
            }
        }

        if (show_lines && show_words && show_bytes)
            snprintf(out, sizeof(out), "%8lu %7lu %7lu\r\n", lines, words, bytes);
        else if (show_lines)
            snprintf(out, sizeof(out), "%8lu\r\n", lines);
        else if (show_words)
            snprintf(out, sizeof(out), "%7lu\r\n", words);
        else
            snprintf(out, sizeof(out), "%7lu\r\n", bytes);
        PROC_PUTS(p, out);
    } else {
        total_lines = 0;
        total_words = 0;
        total_bytes = 0;

        for (i = file_start; i < p->argc; i++) {
            res = f_open(&fil, p->argv[i], FA_READ);
            if (res != FR_OK) {
                PROC_PUTS(p, "wc: cannot open '\"");
                PROC_PUTS(p, p->argv[i]);
                PROC_PUT_LINE(p, "\"");
                continue;
            }

            lines = 0;
            words = 0;
            bytes = 0;
            in_word = 0;

            while ((res = f_read(&fil, buf, sizeof(buf), &br)) == FR_OK && br > 0) {
                for (j = 0; j < br; j++) {
                    unsigned char c = (unsigned char)buf[j];
                    bytes++;
                    if (c == '\n')
                        lines++;
                    if (isspace(c)) {
                        in_word = 0;
                    } else if (!in_word) {
                        in_word = 1;
                        words++;
                    }
                }
            }
            f_close(&fil);

            snprintf(out, sizeof(out), "%8lu %7lu %7lu %s\r\n",
                     lines, words, bytes, p->argv[i]);
            PROC_PUTS(p, out);

            total_lines += lines;
            total_words += words;
            total_bytes += bytes;
        }

        if (p->argc - file_start > 1) {
            if (show_lines && show_words && show_bytes)
                snprintf(out, sizeof(out), "%8lu %7lu %7lu total\r\n",
                         total_lines, total_words, total_bytes);
            else if (show_lines)
                snprintf(out, sizeof(out), "%8lu total\r\n", total_lines);
            else if (show_words)
                snprintf(out, sizeof(out), "%7lu total\r\n", total_words);
            else
                snprintf(out, sizeof(out), "%7lu total\r\n", total_bytes);
            PROC_PUTS(p, out);
        }
    }
    PROC_END(p);
}

int proc_sync_wrapper(saramos_process_t *p)
{
    PROC_BEGIN(p);
    shell_execute(p->argc, p->argv);
    PROC_WAIT(p, !saramos_proc_has_children(p));
    saramos_proc_wait_children(p);
    PROC_END(p);
}
