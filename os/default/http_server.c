#include "http_server.h"

#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"

void http_server_init(void)
{
#if LWIP_HTTPD
    httpd_init();
#endif
}

/*
 * Weak default implementations of the POST and custom-file hooks that
 * LWIP_HTTPD_SUPPORT_POST=1 / LWIP_HTTPD_CUSTOM_FILES=1 require fs.c and
 * httpd.c to link against (see third_party/lwip/src/apps/http/fs.c and
 * httpd.c). These two config flags are shared across every app built on
 * os/default, but only an app that actually wants file upload / custom
 * page serving (currently: apps/drop-a-file) needs real behavior.
 *
 * Defining harmless defaults here -- rather than turning the flags on
 * only for that one app -- keeps every other app (colored-screen,
 * graphical-shell, sudoku, ...) linking unchanged: a strong definition
 * in an app's own .c file (e.g. app_drop_a_file.c) simply overrides
 * these at link time, same pattern as app_register_commands() below in
 * main.c.
 */
#if LWIP_HTTPD_CUSTOM_FILES
__attribute__((weak)) int fs_open_custom(struct fs_file *file, const char *name)
{
    (void)file;
    (void)name;
    return 0; /* not found -- fs_open() falls back to the static fsdata table */
}

__attribute__((weak)) void fs_close_custom(struct fs_file *file)
{
    (void)file;
}
#endif /* LWIP_HTTPD_CUSTOM_FILES */

#if LWIP_HTTPD_SUPPORT_POST
__attribute__((weak)) err_t httpd_post_begin(void *connection, const char *uri,
                                              const char *http_request,
                                              u16_t http_request_len,
                                              int content_len, char *response_uri,
                                              u16_t response_uri_len,
                                              u8_t *post_auto_wnd)
{
    (void)connection;
    (void)uri;
    (void)http_request;
    (void)http_request_len;
    (void)content_len;
    (void)response_uri;
    (void)response_uri_len;
    (void)post_auto_wnd;
    return ERR_ARG; /* deny -- no app has claimed this POST */
}

__attribute__((weak)) err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    (void)connection;
    if (p)
        pbuf_free(p);
    return ERR_OK;
}

__attribute__((weak)) void httpd_post_finished(void *connection, char *response_uri,
                                                u16_t response_uri_len)
{
    (void)connection;
    (void)response_uri;
    (void)response_uri_len;
}
#endif /* LWIP_HTTPD_SUPPORT_POST */
