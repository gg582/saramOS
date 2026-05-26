#ifndef HAL_FIX_H
#define HAL_FIX_H

/* Block toolchain's pthread types to avoid conflicts with saramOS POSIX layer */
#ifndef _SYS__PTHREADTYPES_H_
#define _SYS__PTHREADTYPES_H_
#endif

#ifndef __ASSEMBLER__
/* Standard includes that might be needed by the HAL but we want to control */
#include <sys/types.h>

/* Include saramOS pthread.h from third_party/newlib_posix/include */
/* We assume -I$(POSIX_COMPAT_DIR) is present in CFLAGS */
#include <pthread.h>

/* Now include the platform HAL */
/* The Makefile provides -I$(ROOT_DIR)/include */
#include <hal/esp32_wroom32.h>
#endif

#endif /* HAL_FIX_H */
