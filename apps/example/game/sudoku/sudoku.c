#include "sudoku.h"
#include <stdio.h>

#define SUDOKU_SIZE 4
#define GAME_LINE_SIZE 16

static const int puzzle[SUDOKU_SIZE][SUDOKU_SIZE] = {
    {1, 0, 0, 4},
    {0, 4, 1, 0},
    {0, 1, 4, 0},
    {4, 0, 0, 2},
};

static void copy_puzzle(int board[SUDOKU_SIZE][SUDOKU_SIZE])
{
    for (int r = 0; r < SUDOKU_SIZE; r++) {
        for (int c = 0; c < SUDOKU_SIZE; c++) {
            board[r][c] = puzzle[r][c];
        }
    }
}

static int is_fixed_cell(int row, int col)
{
    return puzzle[row][col] != 0;
}

static void game_read_line(sudoku_puts_fn puts_fn, sudoku_getc_fn getc_fn, char *buf, int size)
{
    int i = 0;

    while (1) {
        int ch = getc_fn();

        if (ch == '\r' || ch == '\n') {
            puts_fn("\r\n");
            buf[i] = '\0';
            return;
        }

        if (ch == '\b' || ch == 127) {
            if (i > 0) {
                i--;
                puts_fn("\b \b");
            }
            continue;
        }

        if (ch >= 32 && ch < 127 && i + 1 < size) {
            buf[i++] = (char)ch;
            char echo[2] = {(char)ch, '\0'};
            puts_fn(echo);
        }
    }
}

static void render_board(sudoku_puts_fn puts_fn, int board[SUDOKU_SIZE][SUDOKU_SIZE])
{
    char line[48];

    puts_fn("\r\n  1 2   3 4\r\n");
    puts_fn(" +---+---+\r\n");

    for (int r = 0; r < SUDOKU_SIZE; r++) {
        snprintf(line, sizeof(line), "%d|", r + 1);
        puts_fn(line);

        for (int c = 0; c < SUDOKU_SIZE; c++) {
            char cell = board[r][c] ? (char)('0' + board[r][c]) : '.';
            snprintf(line, sizeof(line), "%c", cell);
            puts_fn(line);

            if (c == 1 || c == 3) {
                puts_fn("|");
            } else {
                puts_fn(" ");
            }
        }

        puts_fn("\r\n");
        if (r == 1 || r == 3) {
            puts_fn(" +---+---+\r\n");
        }
    }
}

static int allowed_value(int board[SUDOKU_SIZE][SUDOKU_SIZE], int row, int col, int value)
{
    if (value == 0) {
        return 1;
    }

    for (int i = 0; i < SUDOKU_SIZE; i++) {
        if (i != col && board[row][i] == value) {
            return 0;
        }
        if (i != row && board[i][col] == value) {
            return 0;
        }
    }

    int box_row = (row / 2) * 2;
    int box_col = (col / 2) * 2;
    for (int r = box_row; r < box_row + 2; r++) {
        for (int c = box_col; c < box_col + 2; c++) {
            if ((r != row || c != col) && board[r][c] == value) {
                return 0;
            }
        }
    }

    return 1;
}

static int is_solved(int board[SUDOKU_SIZE][SUDOKU_SIZE])
{
    for (int r = 0; r < SUDOKU_SIZE; r++) {
        for (int c = 0; c < SUDOKU_SIZE; c++) {
            if (board[r][c] == 0 || !allowed_value(board, r, c, board[r][c])) {
                return 0;
            }
        }
    }

    return 1;
}

static void print_help(sudoku_puts_fn puts_fn)
{
    puts_fn(
        "\r\nMini Sudoku 4x4\r\n"
        "Enter: row col value, for example: 1 2 3\r\n"
        "Use value 0 to clear a non-fixed cell.\r\n"
        "Commands: h help, r reset, q quit\r\n"
    );
}

void sudoku_run(sudoku_puts_fn puts_fn, sudoku_getc_fn getc_fn)
{
    int board[SUDOKU_SIZE][SUDOKU_SIZE];
    char line[GAME_LINE_SIZE];

    copy_puzzle(board);
    puts_fn("\x1B[2J\x1B[H");
    print_help(puts_fn);

    while (1) {
        render_board(puts_fn, board);

        if (is_solved(board)) {
            puts_fn("\r\nSolved. Press enter to return.\r\n");
            game_read_line(puts_fn, getc_fn, line, sizeof(line));
            return;
        }

        puts_fn("\r\nsudoku> ");
        game_read_line(puts_fn, getc_fn, line, sizeof(line));

        if (line[0] == 'q' || line[0] == 'Q') {
            puts_fn("Leaving Sudoku.\r\n");
            return;
        }

        if (line[0] == 'h' || line[0] == 'H') {
            print_help(puts_fn);
            continue;
        }

        if (line[0] == 'r' || line[0] == 'R') {
            copy_puzzle(board);
            puts_fn("Puzzle reset.\r\n");
            continue;
        }

        int row = 0;
        int col = 0;
        int value = 0;
        if (sscanf(line, "%d %d %d", &row, &col, &value) != 3) {
            puts_fn("Invalid input. Example: 1 2 3\r\n");
            continue;
        }

        if (row < 1 || row > 4 || col < 1 || col > 4 || value < 0 || value > 4) {
            puts_fn("Use row 1-4, column 1-4, value 0-4.\r\n");
            continue;
        }

        row--;
        col--;
        if (is_fixed_cell(row, col)) {
            puts_fn("That cell is fixed.\r\n");
            continue;
        }

        int old_value = board[row][col];
        board[row][col] = value;
        if (!allowed_value(board, row, col, value)) {
            board[row][col] = old_value;
            puts_fn("Move conflicts with row, column, or box.\r\n");
        }
    }
}
