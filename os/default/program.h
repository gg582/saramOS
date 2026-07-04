#ifndef PROGRAM_H
#define PROGRAM_H

#define PROGRAM_MAX_COUNT 8
#define PROGRAM_NAME_SIZE 16
#define PROGRAM_MAX_LINES 64
#define PROGRAM_LINE_SIZE 64

typedef struct {
    int used;
    char name[PROGRAM_NAME_SIZE];
    unsigned int line_count;
    char lines[PROGRAM_MAX_LINES][PROGRAM_LINE_SIZE];
} cli_program_t;

#define PROGRAM_MEM_SIZE 256

struct program_state {
    int acc;
    int flag;
    int mem[PROGRAM_MEM_SIZE];
    const cli_program_t *prog;
};

extern cli_program_t programs[PROGRAM_MAX_COUNT];

cli_program_t *program_get_or_alloc(const char *name);
void program_init_builtins(void);

#endif /* PROGRAM_H */
