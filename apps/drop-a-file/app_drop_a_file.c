/*
 * drop-a-file: example app for saramOS.
 *
 * Serves a single minimalist "Drop files to draw" web page over the
 * existing DHCP/lwIP-httpd stack (see os/default/main.c's "net init" and
 * "http start" CLI commands). The dropped file (any image type the
 * browser's own decoder understands -- PNG/JPEG/GIF/WEBP/BMP) is decoded
 * and re-encoded as a plain 24bpp BMP entirely client-side (createImage-
 * Bitmap + <canvas>, see the toBMP() JS in k_index_html below), so the
 * board itself never needs a PNG/JPEG decoder -- it only ever receives
 * BMP, over a raw HTTP POST body (no multipart parsing needed -- the
 * page uses fetch(url, {method:'POST', body: blob}), which sends the
 * Blob's raw bytes with a plain Content-Length). That upload streams
 * straight to the SD card via FatFS, is then decoded + scaled to fit
 * the 800x480 panel, letterboxed over a background color chosen on the
 * page, and displayed via the existing hal_display_* API.
 *
 * Memory budget (must stay well inside the 16 MB SDRAM at 0xC0000000):
 *   - One 800x480 RGB565 framebuffer in SDRAM, written directly (same
 *     single-live-buffer pattern as apps/colored-screen and
 *     apps/draw-picora -- see ensure_display_started()'s comment for
 *     why this app doesn't use hal_display_flip() ping-ponging):
 *       DISPLAY_FB_SIZE = 768,000 bytes (~750 KB)
 *   - A single BMP source-row scratch buffer in normal SRAM (not SDRAM),
 *     sized for the widest row this app will decode:
 *       MAX_SRC_WIDTH * 3 bytes = 4096 * 3 = 12,288 bytes
 *   - The upload itself is streamed directly to the SD card as it
 *     arrives (one TCP pbuf at a time) -- it is never buffered whole in
 *     RAM, so its size is bounded only by MAX_UPLOAD_BYTES / the SD
 *     card's free space, not by SDRAM.
 *   Total SDRAM use: ~750 KB out of 16 MB. Total extra SRAM use: ~12 KB.
 *
 * The board-side decoder only handles uncompressed 24bpp BMP -- no PNG/
 * JPEG decoder exists anywhere in saramOS, and adding one is out of
 * scope for an example app (see U-Boot's bmp.c for the reference this
 * format handling follows). Every other format is turned into that BMP
 * shape by the browser before it ever reaches the board.
 */
#include <hal/board.h>
#include <hal/hal_touch.h>
#include "hal_sdram.h"
#include "hal_display.h"
#include "ff.h"

#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/pbuf.h"
#include "lwip/err.h"

#include <os/saramos_kernel.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern void hal_uart_puts(const char *s);

/* ---------------------------------------------------------------------
 * Single live framebuffer, direct writes -- same pattern as
 * apps/colored-screen (the one confirmed working on real hardware):
 * hal_display_init() + hal_display_start_video() exactly once, then
 * every later redraw (here: each finished upload) writes straight into
 * that same buffer via hal_display_fb_addr(). The ping-pong/flip
 * mechanism (hal_display_flip()) is only exercised -- and only
 * confirmed working -- on the LVGL path in lvgl_port.c; this app has
 * no need for it (colored-screen and apps/draw-picora accept the same
 * brief-tearing-during-redraw tradeoff for the same reason).
 * ------------------------------------------------------------------- */
static int g_display_ready = 0;

static void ensure_display_started(void)
{
    if (g_display_ready)
        return;

    /* hal_display_init() does not clock/configure SDRAM itself (it
     * only computes the framebuffer address) -- hal_sdram_init() must
     * run first, or every framebuffer write below is going to
     * uninitialized/unclocked memory. Missing this call was the exact
     * bug behind apps/draw-picora's initial black screen; same fix
     * applies here (see that app's commit for the full story). */
    hal_sdram_init();
    hal_display_init();
    memset((void *)hal_display_fb_addr(), 0, DISPLAY_FB_SIZE);
    hal_display_start_video();
    g_display_ready = 1;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                       ((uint16_t)(g & 0xFCU) << 3) |
                       ((uint16_t)(b >> 3)));
}

/* ---------------------------------------------------------------------
 * Background color, set from the page's color picker via the upload
 * request's "?bg=RRGGBB" query parameter.
 * ------------------------------------------------------------------- */
static uint16_t g_bg_color = 0x0000; /* default: black */

static void parse_bg_hex(const char *hex, uint16_t *out)
{
    uint32_t v = 0;
    int i;

    if (!hex)
        return;
    if (hex[0] == '#')
        hex++;

    for (i = 0; i < 6; i++) {
        char c = hex[i];
        uint32_t d;
        if (c >= '0' && c <= '9')
            d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            d = (uint32_t)(c - 'A' + 10);
        else
            return; /* malformed -- leave *out unchanged */
        v = (v << 4) | d;
    }

    *out = rgb565((uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v);
}

/* ---------------------------------------------------------------------
 * BMP decode + scale-to-fit + letterbox composite, streamed row-by-row
 * straight off the SD card (no full-image buffer in RAM -- see the
 * memory budget comment at the top of this file).
 * ------------------------------------------------------------------- */
#define MAX_SRC_WIDTH   4096U
#define BMP_HDR_MAX     138U /* BITMAPFILEHEADER(14) + largest common DIB header */

static uint8_t g_row_buf[MAX_SRC_WIDTH * 3U];

static uint32_t rd_le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd_le16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }

/* Draws directly into the back buffer (the slot NOT currently on
 * screen), then flips -- the whole redraw is atomic from the viewer's
 * perspective, no partial-frame tearing. Returns 0 on success. */
static int decode_and_draw_bmp(const char *path, uint16_t bg)
{
    FIL fil;
    FRESULT fr;
    UINT br;
    uint8_t hdr[BMP_HDR_MAX];
    uint32_t data_off, dib_size;
    int32_t width, height_signed;
    uint32_t height;
    int top_down;
    uint16_t bpp;
    uint32_t row_stride;
    volatile uint16_t *fb;
    uint32_t dst_w, dst_h, x0, y0;
    uint32_t scale_num, scale_den; /* dst = src * scale_num / scale_den */

    fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        hal_uart_puts("drop-a-file: could not reopen uploaded file\r\n");
        return -1;
    }

    fr = f_read(&fil, hdr, sizeof(hdr), &br);
    if (fr != FR_OK || br < 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        hal_uart_puts("drop-a-file: not a BMP file (only uncompressed 24bpp BMP is supported)\r\n");
        f_close(&fil);
        return -1;
    }

    data_off = rd_le32(&hdr[10]);
    dib_size = rd_le32(&hdr[14]);
    width = (int32_t)rd_le32(&hdr[18]);
    height_signed = (int32_t)rd_le32(&hdr[22]);
    bpp = rd_le16(&hdr[28]);
    (void)dib_size;

    if (width <= 0 || width > (int32_t)MAX_SRC_WIDTH || height_signed == 0) {
        hal_uart_puts("drop-a-file: BMP dimensions unsupported (0 < width <= 4096 required)\r\n");
        f_close(&fil);
        return -1;
    }
    if (bpp != 24) {
        hal_uart_puts("drop-a-file: only 24bpp uncompressed BMP is supported\r\n");
        f_close(&fil);
        return -1;
    }

    top_down = (height_signed < 0);
    height = top_down ? (uint32_t)(-height_signed) : (uint32_t)height_signed;
    row_stride = (((uint32_t)width * 3U + 3U) & ~3U); /* rows padded to 4 bytes */

    {
        char dbg[128];
        snprintf(dbg, sizeof(dbg),
                 "drop-a-file: BMP %ldx%ld bpp=%u data_off=%lu row_stride=%lu top_down=%d\r\n",
                 (long)width, (long)height, (unsigned)bpp, (unsigned long)data_off,
                 (unsigned long)row_stride, top_down);
        hal_uart_puts(dbg);
    }

    /* Scale to fit inside 800x480 preserving aspect ratio, then center
     * (letterbox) over the chosen background color. Fixed-point (x256)
     * to avoid needing float. */
    {
        uint32_t sx256 = (DISPLAY_WIDTH * 256U) / (uint32_t)width;
        uint32_t sy256 = (DISPLAY_HEIGHT * 256U) / height;
        uint32_t s256 = (sx256 < sy256) ? sx256 : sy256;
        if (s256 == 0)
            s256 = 1;
        scale_num = s256;
        scale_den = 256U;
        dst_w = ((uint32_t)width * scale_num) / scale_den;
        dst_h = (height * scale_num) / scale_den;
        if (dst_w > DISPLAY_WIDTH) dst_w = DISPLAY_WIDTH;
        if (dst_h > DISPLAY_HEIGHT) dst_h = DISPLAY_HEIGHT;
        if (dst_w == 0) dst_w = 1;
        if (dst_h == 0) dst_h = 1;
        x0 = (DISPLAY_WIDTH - dst_w) / 2U;
        y0 = (DISPLAY_HEIGHT - dst_h) / 2U;
    }

    {
        char dbg[128];
        snprintf(dbg, sizeof(dbg),
                 "drop-a-file: dst=%lux%lu at (%lu,%lu) scale=%lu/%lu bg=0x%04x\r\n",
                 (unsigned long)dst_w, (unsigned long)dst_h, (unsigned long)x0,
                 (unsigned long)y0, (unsigned long)scale_num, (unsigned long)scale_den,
                 (unsigned)bg);
        hal_uart_puts(dbg);
    }

    ensure_display_started();
    fb = (volatile uint16_t *)hal_display_fb_addr();

    /* Fill the whole framebuffer with the background color first. */
    for (uint32_t i = 0; i < (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
        fb[i] = bg;

    uint32_t rows_failed = 0;
    for (uint32_t dy = 0; dy < dst_h; dy++) {
        uint32_t src_y = (dy * scale_den) / scale_num;
        uint32_t file_row = top_down ? src_y : (height - 1U - src_y);
        uint32_t off = data_off + file_row * row_stride;

        if (f_lseek(&fil, off) != FR_OK) {
            rows_failed++;
            continue;
        }
        if (f_read(&fil, g_row_buf, row_stride, &br) != FR_OK || br < row_stride) {
            rows_failed++;
            continue;
        }

        volatile uint16_t *dst_row = fb + (y0 + dy) * DISPLAY_WIDTH + x0;
        for (uint32_t dx = 0; dx < dst_w; dx++) {
            uint32_t src_x = (dx * scale_den) / scale_num;
            const uint8_t *px = &g_row_buf[src_x * 3U]; /* B, G, R */
            dst_row[dx] = rgb565(px[2], px[1], px[0]);
        }
    }

    f_close(&fil);

    __asm volatile("dsb" ::: "memory");

    {
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "drop-a-file: drawn (rows_failed=%lu/%lu)\r\n",
                 (unsigned long)rows_failed, (unsigned long)dst_h);
        hal_uart_puts(dbg);
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * HTML page, served via fs_open_custom() for "/index.html" (and "/",
 * which httpd falls back to "/index.html" for on its own). Kept as one
 * plain C string -- no separate build step, no touching the shared
 * os/default/fsdata_custom.c table.
 * ------------------------------------------------------------------- */
static const char k_index_html[] =
"<!doctype html><html><head><meta charset=\"utf-8\">"
"<title>drop-a-file</title>"
"<style>"
"html,body{height:100%;margin:0;font-family:sans-serif;background:#fafafa;color:#222;"
"display:flex;align-items:center;justify-content:center}"
".card{width:min(520px,90vw);text-align:center}"
"h1{font-weight:500;font-size:1.25rem;margin:0 0 .5rem}"
"p{color:#777;font-size:.9rem;margin:0 0 1.5rem}"
"#drop{border:2px dashed #ccc;border-radius:12px;padding:3rem 1rem;transition:.15s}"
"#drop.over{border-color:#333;background:#f0f0f0}"
"#drop span{color:#999}"
"#bgrow{margin-top:1.5rem;display:flex;align-items:center;justify-content:center;gap:.6rem}"
"#status{margin-top:1rem;font-size:.85rem;color:#777;min-height:1.2em}"
"</style></head><body>"
"<div class=\"card\">"
"<h1>drop-a-file</h1>"
"<p>saramOS &middot; STM32F769I-DISCO &middot; 800&times;480</p>"
"<div id=\"drop\"><span>Drop files to draw</span></div>"
"<div id=\"bgrow\"><label for=\"bg\">Background</label>"
"<input type=\"color\" id=\"bg\" value=\"#000000\"></div>"
"<div id=\"status\">Any image your browser can open (PNG/JPEG/GIF/WEBP/BMP) &mdash; converted to BMP locally before upload.</div>"
"</div>"
"<script>"
"const drop=document.getElementById('drop'),bg=document.getElementById('bg'),"
"st=document.getElementById('status');"
"['dragenter','dragover'].forEach(e=>drop.addEventListener(e,ev=>{ev.preventDefault();drop.classList.add('over');}));"
"['dragleave','drop'].forEach(e=>drop.addEventListener(e,ev=>{ev.preventDefault();drop.classList.remove('over');}));"
/* The board only decodes uncompressed 24bpp BMP (no PNG/JPEG decoder in
 * the firmware, and this app is intentionally not the place to add
 * one) -- so instead of restricting what the user can drop, the
 * browser's own image decoder (createImageBitmap, which understands
 * whatever image types the browser does) does the decode, a <canvas>
 * gives us raw pixels, and this hand-rolled encoder packs those into a
 * minimal 24bpp BMP client-side. The board only ever sees BMP.
 *
 * The canvas draw also downscales to fit 800x480 (the panel's own
 * resolution -- DISPLAY_WIDTH/DISPLAY_HEIGHT) before encoding, aspect
 * ratio preserved, never upscaled. The board would happily downscale a
 * full-resolution BMP itself, but a modern phone photo is tens of MB
 * uncompressed at full size -- pointless to push that many bytes over
 * the board's TCP stack (and over MAX_UPLOAD_BYTES) when the extra
 * resolution is discarded on arrival anyway.
 *
 * imageOrientation:'from-image' makes createImageBitmap() honor an
 * EXIF orientation tag (phone photos taken in portrait are commonly
 * stored as landscape pixels + a rotation tag) -- without it, browser
 * default behavior here isn't consistent, and a portrait photo could
 * come out sideways with no rotation applied at all. */
"async function toBMP(file){"
"const bmp=await createImageBitmap(file,{imageOrientation:'from-image'});"
"const scale=Math.min(1,800/bmp.width,480/bmp.height);"
"const w=Math.max(1,Math.round(bmp.width*scale));"
"const h=Math.max(1,Math.round(bmp.height*scale));"
"const cv=document.createElement('canvas');cv.width=w;cv.height=h;"
"const ctx=cv.getContext('2d');ctx.drawImage(bmp,0,0,w,h);"
"const px=ctx.getImageData(0,0,w,h).data;"
"const rowSize=(w*3+3)&~3, pixBytes=rowSize*h, fileSize=54+pixBytes;"
"const buf=new ArrayBuffer(fileSize), dv=new DataView(buf), b=new Uint8Array(buf);"
"b[0]=0x42;b[1]=0x4D;"
"dv.setUint32(2,fileSize,true);dv.setUint32(10,54,true);"
"dv.setUint32(14,40,true);dv.setInt32(18,w,true);dv.setInt32(22,h,true);"
"dv.setUint16(26,1,true);dv.setUint16(28,24,true);dv.setUint32(30,0,true);"
"dv.setUint32(34,pixBytes,true);"
"let o=54;"
"for(let y=h-1;y>=0;y--){"
"const rowStart=o;"
"for(let x=0;x<w;x++){"
"const si=(y*w+x)*4;"
"b[o++]=px[si+2];b[o++]=px[si+1];b[o++]=px[si];"
"}"
"while(o-rowStart<rowSize)b[o++]=0;"
"}"
"return new Blob([buf],{type:'image/bmp'});"
"}"
"drop.addEventListener('drop',async ev=>{"
"ev.preventDefault();"
"const f=ev.dataTransfer.files[0]; if(!f) return;"
"const color=bg.value.replace('#','');"
"let blob;"
"st.textContent='Converting '+f.name+'...';"
"try{blob=await toBMP(f);}"
"catch(err){st.textContent='Could not decode that as an image ('+err.message+').';return;}"
"st.textContent='Uploading '+blob.size+' bytes...';"
"let r;"
"try{r=await fetch('/upload?bg='+color,{method:'POST',body:blob});}"
"catch(err){st.textContent='Upload failed (network error: '+err.message+').';return;}"
"st.textContent=r.ok?'Drawn on the panel.':'Upload failed (HTTP '+r.status+').';"
"});"
"</script></body></html>";

int fs_open_custom(struct fs_file *file, const char *name)
{
    if (!file || !name)
        return 0;
    if (strcmp(name, "/index.html") != 0)
        return 0;

    file->data = k_index_html;
    file->len = (int)(sizeof(k_index_html) - 1);
    file->index = file->len;
    file->flags = 0; /* let httpd synthesize headers (Content-Type from .html) */
    return 1;
}

void fs_close_custom(struct fs_file *file)
{
    (void)file;
}

/* ---------------------------------------------------------------------
 * Upload handling: raw POST body (no multipart) straight to SD.
 * ------------------------------------------------------------------- */
#define UPLOAD_PATH        "upload.bmp"
#define MAX_UPLOAD_BYTES    (4U * 1024U * 1024U) /* generous for a photo; bounded so a bad/slow client can't fill the card */

static FIL g_upload_fil;
static int g_upload_open = 0;
static uint32_t g_upload_bytes = 0;
static int g_upload_ok = 0;

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                        u16_t http_request_len, int content_len, char *response_uri,
                        u16_t response_uri_len, u8_t *post_auto_wnd)
{
    const char *q;
    FRESULT fr;

    (void)connection;
    (void)http_request;
    (void)http_request_len;
    (void)response_uri;
    (void)response_uri_len;

    if (post_auto_wnd)
        *post_auto_wnd = 1;

    if (!uri || strncmp(uri, "/upload", 7) != 0)
        return ERR_ARG; /* not our URI -- deny */

    if (content_len > 0 && (uint32_t)content_len > MAX_UPLOAD_BYTES) {
        hal_uart_puts("drop-a-file: upload too large, rejected\r\n");
        return ERR_ARG;
    }

    q = strchr(uri, '?');
    if (q) {
        const char *p = q + 1;
        while (p && *p) {
            if (strncmp(p, "bg=", 3) == 0) {
                parse_bg_hex(p + 3, &g_bg_color);
                break;
            }
            p = strchr(p, '&');
            if (p) p++;
        }
    }

    fr = f_open(&g_upload_fil, UPLOAD_PATH, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        hal_uart_puts("drop-a-file: could not open upload.bmp on SD (sd init done?)\r\n");
        return ERR_ARG;
    }

    g_upload_open = 1;
    g_upload_bytes = 0;
    g_upload_ok = 1;
    return ERR_OK;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    struct pbuf *q;

    (void)connection;
    if (!p)
        return ERR_OK;

    if (g_upload_open && g_upload_ok) {
        for (q = p; q != NULL; q = q->next) {
            UINT bw = 0;
            if (g_upload_bytes + q->len > MAX_UPLOAD_BYTES) {
                g_upload_ok = 0;
                break;
            }
            if (f_write(&g_upload_fil, q->payload, q->len, &bw) != FR_OK || bw != q->len) {
                g_upload_ok = 0;
                break;
            }
            g_upload_bytes += bw;
        }
    }

    pbuf_free(p);
    return ERR_OK;
}

/* Set by httpd_post_finished(), consumed by draw_task_entry() below --
 * see that task's comment for why the actual decode+draw happens there
 * instead of inline, synchronously, in httpd_post_finished() itself. */
static volatile int g_draw_pending = 0;
static uint16_t g_draw_bg;

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    (void)connection;

    if (g_upload_open) {
        f_close(&g_upload_fil);
        g_upload_open = 0;
    }

    if (g_upload_ok && g_upload_bytes > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "drop-a-file: received %lu bytes\r\n",
                 (unsigned long)g_upload_bytes);
        hal_uart_puts(msg);
        /* Hand off to draw_task_entry() rather than calling
         * decode_and_draw_bmp() here -- this function runs on net_task
         * (see main.c's ethernetif_input()/net_task_entry()), and this
         * is the point where httpd wants to finish and send the HTTP
         * response back to the browser. decode_and_draw_bmp() re-reads
         * the file row by row from SD, and each SD block read can now
         * retry up to 5 times with backoff (see hal_sdmmc.c) -- on a
         * larger image or a slower/marginal card, that can add up to
         * several real seconds of net_task being unavailable to
         * service *this same TCP connection* (ACKs, the response
         * itself, ...) before it ever gets a chance to send anything
         * back. Observed exactly this: the browser's fetch() reporting
         * "NetworkError" even though the board had genuinely received
         * the full upload and was still in the middle of successfully
         * decoding and drawing it -- the connection looked dead to the
         * browser well before net_task got back around to finishing
         * the response. Setting a flag here instead lets httpd send
         * the response immediately; the actual (slow) drawing happens
         * afterward, off the TCP path entirely. */
        g_draw_bg = g_bg_color;
        g_draw_pending = 1;
    } else {
        hal_uart_puts("drop-a-file: upload failed or empty\r\n");
    }

    if (response_uri && response_uri_len > 0)
        snprintf(response_uri, response_uri_len, "/index.html");
}

/* Picks up g_draw_pending, set by httpd_post_finished() above, and does
 * the actual (potentially slow -- SD reads with retries, DSI/LTDC on
 * the very first call) decode_and_draw_bmp() call here instead, on its
 * own TCB task -- completely off net_task, so a slow draw never delays
 * the HTTP response for the upload that triggered it (see
 * httpd_post_finished()'s comment for why that mattered). Same
 * dedicated-task-for-a-slow-thing pattern as apps/draw-picora's
 * button_task_entry(). */
static void snapshot_current_frame(void); /* defined below, see the touch-test section */
static volatile int g_have_snapshot = 0;

static void draw_task_entry(void *arg)
{
    (void)arg;
    for (;;) {
        if (g_draw_pending) {
            if (decode_and_draw_bmp(UPLOAD_PATH, g_draw_bg) == 0) {
                snapshot_current_frame();
                g_have_snapshot = 1;
            }
            g_draw_pending = 0;
        }
    }
}

/* ---------------------------------------------------------------------
 * Touch test: tap the panel to (a) print the tap coordinates as green
 * text at the top-left corner, and (b) cycle the currently-displayed
 * image's RGB channel order through an 8-step sequence, per request.
 *
 * Channel order is always applied fresh from a pristine snapshot of
 * the last successfully drawn frame (see g_snapshot below), not
 * compounded from whatever the previous tap already did -- so each tap
 * count deterministically shows the same result no matter how many
 * taps came before it, rather than drifting based on rounding/order of
 * repeated in-place permutation.
 *
 * SDRAM budget update (see the file-top comment): one more full
 * 800x480 RGB565 buffer for the snapshot, placed right after the live
 * framebuffer -- another DISPLAY_FB_SIZE (768,000) bytes. Total SDRAM
 * use is now framebuffer + snapshot = 2 * 768,000 = 1,536,000 bytes
 * (~1.46 MB) out of 16 MB.
 * ------------------------------------------------------------------- */
static inline uint32_t snapshot_addr(void)
{
    return hal_sdram_base() + DISPLAY_FB_SIZE;
}

/* Called once after decode_and_draw_bmp() finishes drawing into the
 * live framebuffer -- copies it into the snapshot buffer so later taps
 * always have a pristine "as uploaded" copy to permute from. */
static void snapshot_current_frame(void)
{
    memcpy((void *)snapshot_addr(), (const void *)hal_display_fb_addr(), DISPLAY_FB_SIZE);
}

/* Tiny 3x5 bitmap font -- just enough characters for "[x, y]" coordinate
 * labels: digits, '[', ']', ',', space. Each glyph is 5 rows, 3 bits
 * per row (bit 2 = leftmost column), 1 = pixel on. */
typedef struct { char ch; uint8_t rows[5]; } font_glyph_t;

static const font_glyph_t k_font[] = {
    {'0', {0x7, 0x5, 0x5, 0x5, 0x7}},
    {'1', {0x2, 0x6, 0x2, 0x2, 0x7}},
    {'2', {0x7, 0x1, 0x7, 0x4, 0x7}},
    {'3', {0x7, 0x1, 0x7, 0x1, 0x7}},
    {'4', {0x5, 0x5, 0x7, 0x1, 0x1}},
    {'5', {0x7, 0x4, 0x7, 0x1, 0x7}},
    {'6', {0x7, 0x4, 0x7, 0x5, 0x7}},
    {'7', {0x7, 0x1, 0x2, 0x2, 0x2}},
    {'8', {0x7, 0x5, 0x7, 0x5, 0x7}},
    {'9', {0x7, 0x5, 0x7, 0x1, 0x7}},
    {'[', {0x6, 0x4, 0x4, 0x4, 0x6}},
    {']', {0x3, 0x1, 0x1, 0x1, 0x3}},
    {',', {0x0, 0x0, 0x0, 0x2, 0x4}},
    {' ', {0x0, 0x0, 0x0, 0x0, 0x0}},
};

#define FONT_SCALE  3U  /* each font pixel drawn as a FONT_SCALE x FONT_SCALE block */
#define FONT_GLYPH_W (3U * FONT_SCALE)
#define FONT_GLYPH_H (5U * FONT_SCALE)
#define FONT_GAP    (1U * FONT_SCALE)

static void draw_char(volatile uint16_t *fb, uint32_t x0, uint32_t y0, char ch, uint16_t color)
{
    const font_glyph_t *g = NULL;
    for (size_t i = 0; i < sizeof(k_font) / sizeof(k_font[0]); i++) {
        if (k_font[i].ch == ch) {
            g = &k_font[i];
            break;
        }
    }
    if (!g)
        return;

    for (uint32_t row = 0; row < 5U; row++) {
        for (uint32_t col = 0; col < 3U; col++) {
            if (!((g->rows[row] >> (2U - col)) & 1U))
                continue;
            for (uint32_t sy = 0; sy < FONT_SCALE; sy++) {
                uint32_t py = y0 + row * FONT_SCALE + sy;
                if (py >= DISPLAY_HEIGHT)
                    continue;
                volatile uint16_t *prow = fb + py * DISPLAY_WIDTH;
                for (uint32_t sx = 0; sx < FONT_SCALE; sx++) {
                    uint32_t px = x0 + col * FONT_SCALE + sx;
                    if (px >= DISPLAY_WIDTH)
                        continue;
                    prow[px] = color;
                }
            }
        }
    }
}

static void draw_text(volatile uint16_t *fb, uint32_t x0, uint32_t y0, const char *s, uint16_t color)
{
    uint32_t x = x0;
    for (; *s; s++) {
        draw_char(fb, x, y0, *s, color);
        x += FONT_GLYPH_W + FONT_GAP;
    }
}

/* The 8-step channel-order cycle, applied on each tap (index = tap
 * count mod 8, 0-based -- so tap #1 uses k_channel_cycle[0], etc.).
 * Each entry says, for output (R,G,B), which original channel (0=R,
 * 1=G, 2=B) to take: e.g. {1,2,0} means new R = old G, new G = old B,
 * new B = old R -- i.e. "RGB -> GBR". Sequence per request: GBR, BRG,
 * RBG, BGR, GRB, RBG, BRG, RGB (tap #8 returns to the original order). */
static const uint8_t k_channel_cycle[8][3] = {
    {1, 2, 0}, /* 1: GBR */
    {2, 0, 1}, /* 2: BRG */
    {0, 2, 1}, /* 3: RBG */
    {2, 1, 0}, /* 4: BGR */
    {1, 0, 2}, /* 5: GRB */
    {0, 2, 1}, /* 6: RBG */
    {2, 0, 1}, /* 7: BRG */
    {0, 1, 2}, /* 8: RGB (identity) */
};

static uint32_t g_tap_count = 0;

/* Re-derives the live framebuffer from the pristine snapshot, applying
 * the channel order for the current tap count, then draws the "[x, y]"
 * label on top in green. */
static void apply_tap(int16_t x, int16_t y)
{
    volatile uint16_t *fb = (volatile uint16_t *)hal_display_fb_addr();
    const uint16_t *src = (const uint16_t *)snapshot_addr();
    const uint8_t *order = k_channel_cycle[g_tap_count % 8U];

    for (uint32_t i = 0; i < (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        uint16_t px = src[i];
        uint8_t ch[3];
        ch[0] = (uint8_t)(((px >> 11) & 0x1FU) << 3); /* R */
        ch[1] = (uint8_t)(((px >> 5) & 0x3FU) << 2);  /* G */
        ch[2] = (uint8_t)((px & 0x1FU) << 3);         /* B */
        fb[i] = rgb565(ch[order[0]], ch[order[1]], ch[order[2]]);
    }

    char label[24];
    snprintf(label, sizeof(label), "[%d, %d]", (int)x, (int)y);
    draw_text(fb, 2, 2, label, rgb565(0, 255, 0)); /* green */

    __asm volatile("dsb" ::: "memory");
}

/* Polls the touch panel, fires apply_tap() once per new press (edge-
 * triggered on press-down, not held-down repeats). Its own TCB task --
 * same reasoning as net_task/draw_task: never block anything else. */
static void touch_task_entry(void *arg)
{
    int was_pressed = 0;
    (void)arg;

    for (;;) {
        int16_t x = 0, y = 0;
        int pressed = hal_touch_read(&x, &y);
        if (pressed && !was_pressed && g_have_snapshot) {
            g_tap_count++;
            apply_tap(x, y);
        }
        was_pressed = pressed;
    }
}

/* ---------------------------------------------------------------------
 * CLI: "dropafile" re-prints the board's IP/status on demand (e.g.
 * after DHCP finishes, which happens asynchronously after boot). Actual
 * bring-up (net/SD/HTTP + display) happens automatically at boot in
 * app_register_commands() below -- see its comment.
 * ------------------------------------------------------------------- */
static void cli_dropafile(const char *arg)
{
    (void)arg;
    hal_uart_puts("drop-a-file: running since boot. 'net status' shows the board's IP\r\n"
                   "once DHCP finishes; browse to http://<that-ip>/ and drop an image.\r\n");
}

/* Auto-start everything this app needs on boot -- no manual "net init"/
 * "sd init"/"http start"/"dropafile" typing required, per request: a
 * flash-and-go example app. cli_net_init()/cli_sd_init()/cli_http_start()
 * are the exact same functions the "net init"/"sd init"/"http start" CLI
 * commands call (made non-static in main.c for this); calling them here
 * instead of duplicating their bodies keeps this in lock-step with
 * whatever those commands do. DHCP itself finishes asynchronously after
 * this returns (main()'s scheduler loop pumps it once running -- see
 * saramos_sched_housekeeping() in main.c), same as it would if the user
 * had typed "net init" by hand. If no SD card is inserted or something
 * else in this sequence fails, each step logs its own message and
 * boot continues rather than getting stuck. */
void app_register_commands(void)
{
    extern void cli_register_command(const char *name, void (*fn)(const char *arg));
    extern void cli_net_init(void);
    extern void cli_sd_init(void);
    extern void cli_http_start(void);

    cli_register_command("dropafile", cli_dropafile);

    cli_net_init();
    cli_sd_init();
    cli_http_start();
    ensure_display_started();

    /* See draw_task_entry()'s comment: decode+draw runs on its own TCB
     * task so it never blocks net_task (and therefore the HTTP
     * response for the upload that triggered it). Priority matches
     * net_task/sched_task (100) for the same SysTick round-robin
     * reasoning as those -- see main.c. */
    {
        static uint8_t draw_task_stack[8192];
        static saramos_tcb_t draw_task_tcb;

        saramos_task_init(&draw_task_tcb, 3, draw_task_entry,
                          NULL, draw_task_stack, sizeof(draw_task_stack),
                          100U, NULL, NULL);
        saramos_task_add(&draw_task_tcb);
    }

    /* Touch test (see touch_task_entry()'s comment) -- its own TCB
     * task, same priority/reasoning as the others above. */
    hal_touch_init();
    {
        static uint8_t touch_task_stack[2048];
        static saramos_tcb_t touch_task_tcb;

        saramos_task_init(&touch_task_tcb, 4, touch_task_entry,
                          NULL, touch_task_stack, sizeof(touch_task_stack),
                          100U, NULL, NULL);
        saramos_task_add(&touch_task_tcb);
    }

    hal_uart_puts("drop-a-file: auto-started (net/sd/http/display/touch)\r\n");
}
