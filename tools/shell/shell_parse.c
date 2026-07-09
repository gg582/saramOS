/*
 * Tokenizer, variable expansion, and simple wildcard globbing for the
 * enhanced saramOS shell.
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "shell.h"
#include "ff.h"

extern void hal_uart_puts(const char *s);

static int match_char_class(char c, const char *pat, size_t *skip)
{
    int neg = 0;
    size_t i = 0;
    if (pat[i] == '!') {
        neg = 1;
        i++;
    }
    int matched = 0;
    while (pat[i] && pat[i] != ']') {
        if (pat[i + 1] == '-' && pat[i + 2] && pat[i + 2] != ']') {
            if (c >= pat[i] && c <= pat[i + 2])
                matched = 1;
            i += 3;
        } else {
            if (c == pat[i])
                matched = 1;
            i++;
        }
    }
    *skip = i + 1; /* include ']' */
    return neg ? !matched : matched;
}

static int glob_match(const char *name, const char *pat)
{
    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat)
                return 1;
            while (*name) {
                if (glob_match(name, pat))
                    return 1;
                name++;
            }
            return 0;
        } else if (*pat == '?') {
            if (!*name)
                return 0;
            pat++;
            name++;
        } else if (*pat == '[') {
            size_t skip;
            if (!match_char_class(*name, pat + 1, &skip))
                return 0;
            pat += skip + 1;
            name++;
        } else {
            if (*name != *pat)
                return 0;
            pat++;
            name++;
        }
    }
    return *name == '\0';
}

int shell_glob(const char *pattern, char matches[][SHELL_LINE_SIZE], int max_matches)
{
    DIR dir;
    FILINFO fno;
    int count = 0;

    if (f_opendir(&dir, ".") != FR_OK)
        return 0;

    while (count < max_matches) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
            break;
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;
        if (glob_match(fno.fname, pattern)) {
            strncpy(matches[count], fno.fname, SHELL_LINE_SIZE - 1);
            matches[count][SHELL_LINE_SIZE - 1] = '\0';
            count++;
        }
    }

    f_closedir(&dir);
    return count;
}

static const char *find_var_end(const char *p)
{
    if (*p == '{') {
        const char *e = strchr(p + 1, '}');
        return e ? e : p;
    }
    if (isalpha((unsigned char)*p) || *p == '_') {
        p++;
        while (isalnum((unsigned char)*p) || *p == '_')
            p++;
        return p;
    }
    return p + 1;
}

static void copy_var(const char *name, size_t namelen, char *dst, size_t *di, size_t size)
{
    char var_name[64];
    if (namelen >= sizeof(var_name))
        namelen = sizeof(var_name) - 1;
    strncpy(var_name, name, namelen);
    var_name[namelen] = '\0';

    char value[SHELL_ENV_SIZE];
    if (shell_env_get(var_name, value, sizeof(value))) {
        size_t vlen = strlen(value);
        if (*di + vlen >= size)
            vlen = size - *di - 1;
        memcpy(dst + *di, value, vlen);
        *di += vlen;
    }
}

int shell_expand(const char *src, char *dst, size_t size)
{
    size_t di = 0;
    for (const char *p = src; *p && di < size - 1; ) {
        if (*p == '$') {
            p++;
            if (*p == '{') {
                const char *e = strchr(p + 1, '}');
                if (e) {
                    copy_var(p + 1, (size_t)(e - p - 1), dst, &di, size);
                    p = e + 1;
                } else {
                    dst[di++] = '$';
                }
            } else {
                const char *e = find_var_end(p);
                copy_var(p, (size_t)(e - p), dst, &di, size);
                p = e;
            }
        } else {
            dst[di++] = *p++;
        }
    }
    dst[di] = '\0';
    return 1;
}

int shell_tokenize(char *line, char *tokens[], int max_tokens)
{
    int count = 0;
    int i = 0;

    while (line[i] && count < max_tokens) {
        while (line[i] == ' ' || line[i] == '\t')
            i++;
        if (!line[i])
            break;

        if (line[i] == ';' || line[i] == '\n') {
            line[i] = '\0';
            i++;
            continue;
        }

        int quote = 0;
        if (line[i] == '\'' || line[i] == '"')
            quote = line[i++];

        tokens[count++] = &line[i];

        while (line[i]) {
            if (quote) {
                if (line[i] == quote) {
                    line[i] = '\0';
                    i++;
                    quote = 0;
                    break;
                }
            } else {
                if (line[i] == ' ' || line[i] == '\t' ||
                    line[i] == ';' || line[i] == '\n') {
                    int end = line[i];
                    line[i] = '\0';
                    i++;
                    if (end == ';' || end == '\n')
                        break;
                    break;
                }
            }
            i++;
        }
    }

    return count;
}
