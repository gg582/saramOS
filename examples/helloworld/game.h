#ifndef GAME_H
#define GAME_H

typedef void (*game_puts_fn)(const char *s);
typedef int (*game_getc_fn)(void);

void game_run(game_puts_fn puts_fn, game_getc_fn getc_fn);

#endif /* GAME_H */
