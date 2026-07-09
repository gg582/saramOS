#ifndef SARAMOS_PIPE_H
#define SARAMOS_PIPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <os/saramos_process.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SARAMOS_PIPE_SIZE
#define SARAMOS_PIPE_SIZE 256U
#endif

typedef struct saramos_pipe {
    uint8_t buf[SARAMOS_PIPE_SIZE];
    uint16_t head;  /* write position */
    uint16_t tail;  /* read position */
    uint16_t count;
    bool eof;

    /* Writers waiting for space. */
    saramos_process_t *writers;
    /* Readers waiting for data. */
    saramos_process_t *readers;
} saramos_pipe_t;

void saramos_pipe_init(saramos_pipe_t *pipe);
void saramos_pipe_close(saramos_pipe_t *pipe);

/* Non-blocking byte operations. */
int  saramos_pipe_write_byte(saramos_pipe_t *pipe, uint8_t c);
int  saramos_pipe_read_byte(saramos_pipe_t *pipe);
size_t saramos_pipe_write(saramos_pipe_t *pipe, const uint8_t *data, size_t len);
size_t saramos_pipe_read(saramos_pipe_t *pipe, uint8_t *data, size_t len);

/* Status. */
size_t saramos_pipe_avail(const saramos_pipe_t *pipe);
size_t saramos_pipe_space(const saramos_pipe_t *pipe);
bool saramos_pipe_eof(const saramos_pipe_t *pipe);

/* Blocking helpers that yield. Use inside a process function. */
#define PROC_PIPE_WRITE(p, pipe, data, len) \
    do { (p)->io.wr_ptr = (const uint8_t *)(data); \
         (p)->io.wr_remain = (len); \
         while ((p)->io.wr_remain > 0) { \
             size_t _w = saramos_pipe_write((pipe), (p)->io.wr_ptr, (p)->io.wr_remain); \
             if (_w == 0) { if ((pipe)->eof) break; saramos_pipe_wait_writer((pipe), (p)); PROC_YIELD(p); continue; } \
             (p)->io.wr_ptr += _w; \
             (p)->io.wr_remain -= _w; \
         } \
    } while (0)

#define PROC_PIPE_READ(p, pipe, data, len, out_count) \
    do { *(out_count) = 0; \
         (p)->io.rd_ptr = (uint8_t *)(data); \
         (p)->io.rd_remain = (len); \
         while ((p)->io.rd_remain > 0) { \
             size_t _r = saramos_pipe_read((pipe), (p)->io.rd_ptr, (p)->io.rd_remain); \
             if (_r == 0) { if ((pipe)->eof) break; saramos_pipe_wait_reader((pipe), (p)); PROC_YIELD(p); continue; } \
             (p)->io.rd_ptr += _r; \
             (p)->io.rd_remain -= _r; \
             *(out_count) += _r; \
         } \
    } while (0)

#define PROC_PIPE_READ_LINE(p, pipe, buf, size, out_len) \
    do { *(out_len) = 0; \
         (p)->io.rd_line_i = 0; \
         while ((p)->io.rd_line_i + 1 < (size)) { \
             int _c = saramos_pipe_read_byte((pipe)); \
             if (_c < 0) { if ((pipe)->eof) break; saramos_pipe_wait_reader((pipe), (p)); PROC_YIELD(p); continue; } \
             (buf)[(p)->io.rd_line_i++] = (char)_c; \
             if (_c == '\n') break; \
         } \
         if ((p)->io.rd_line_i < (size)) (buf)[(p)->io.rd_line_i] = '\0'; \
         *(out_len) = (p)->io.rd_line_i; \
    } while (0)

/* Register a process as waiting for space/data. */
void saramos_pipe_wait_writer(saramos_pipe_t *pipe, saramos_process_t *p);
void saramos_pipe_wait_reader(saramos_pipe_t *pipe, saramos_process_t *p);

/* Wake waiters when pipe state changes. */
void saramos_pipe_wake_writers(saramos_pipe_t *pipe);
void saramos_pipe_wake_readers(saramos_pipe_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* SARAMOS_PIPE_H */
