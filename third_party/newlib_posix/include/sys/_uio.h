#ifndef _SYS__UIO_H_
#define _SYS__UIO_H_

#include <sys/_iovec.h>
#include <sys/_types.h>

enum uio_rw {
    UIO_READ,
    UIO_WRITE
};

enum uio_seg {
    UIO_USERSPACE,
    UIO_SYSSPACE,
    UIO_NOCOPY
};

struct uio {
    struct iovec *uio_iov;
    int           uio_iovcnt;
    off_t         uio_offset;
    ssize_t       uio_resid;
};

#endif /* _SYS__UIO_H_ */
