#include "http_server.h"

#include "lwip/opt.h"
#include "lwip/apps/httpd.h"

void http_server_init(void)
{
#if LWIP_HTTPD
    httpd_init();
#endif
}
