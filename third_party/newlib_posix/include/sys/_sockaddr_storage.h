#ifndef _SYS__SOCKADDR_STORAGE_H_
#define _SYS__SOCKADDR_STORAGE_H_

#include <stdint.h>

struct sockaddr_storage {
    uint8_t  ss_len;
    uint8_t  ss_family;
    uint8_t  __ss_pad1[6];
    int64_t  __ss_align;
    uint8_t  __ss_pad2[112];
};

#endif /* _SYS__SOCKADDR_STORAGE_H_ */
