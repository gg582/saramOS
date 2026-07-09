#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#if defined(BOARD_STM32F769I_DISCO)
#include <hal/stm32f769i-disco.h>
#elif defined(BOARD_STM32F769I_DISC1)
#include <hal/stm32f769i-disc1.h>
#else
#error "BOARD_* macro not defined. Set BOARD=stm32f769i-disco or BOARD=stm32f769i-disc1."
#endif

#endif /* HAL_BOARD_H */
