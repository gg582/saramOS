#ifndef _SYS__IOVEC_H_
#define _SYS__IOVEC_H_

#include <sys/_types.h>

struct iovec {
    void   *iov_base;
    size_t  iov_len;
};

#endif /* _SYS__IOVEC_H_ */
