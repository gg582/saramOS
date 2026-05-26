# FreeBSD POSIX compatibility headers

`saramOS` needs a POSIX surface (poll, sockets, network byte order helpers) when
building libTTAK for non-Linux targets such as ESP32. Instead of keeping
hand-written stubs that silently return errors, we reuse the real header
definitions shipped with the FreeBSD libc (stable/14 branch). The Xtensa
newlib toolchain and lwIP implementations provide the functions at link time;
these headers simply expose the correct types and macros when the vendor
toolchain does not ship them.

Files copied from https://github.com/freebsd/freebsd-src/tree/stable/14
on 2026-04-03:

- `include/sys/socket.h`
- `include/sys/poll.h`
- `include/sys/uio.h`
- `include/netinet/in.h`
- `include/arpa/inet.h`
- `include/poll.h` (trivial shim including `<sys/poll.h>`)

See `LICENSE` for licensing details.
