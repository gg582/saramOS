#ifndef _MACHINE__ALIGN_H_
#define _MACHINE__ALIGN_H_

#include <stdint.h>

#define _ALIGNBYTES	(sizeof(long) - 1)
#define _ALIGN(p)	(((uintptr_t)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)

#endif /* _MACHINE__ALIGN_H_ */
