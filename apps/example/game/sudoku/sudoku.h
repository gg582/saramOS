#ifndef SUDOKU_H
#define SUDOKU_H

typedef void (*sudoku_puts_fn)(const char *s);
typedef int (*sudoku_getc_fn)(void);

void sudoku_run(sudoku_puts_fn puts_fn, sudoku_getc_fn getc_fn);

#endif /* SUDOKU_H */
