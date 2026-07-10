/*
 * Minimal LVGL v9 type definitions.
 */
#ifndef LV_TYPES_H
#define LV_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LV_STDLIB_BUILTIN       0
#define LV_STDLIB_CLIB          1
#define LV_STDLIB_MICROPYTHON   2
#define LV_STDLIB_RTTHREAD      3

#define LV_OS_NONE              0
#define LV_OS_PTHREAD           1
#define LV_OS_FREERTOS          2
#define LV_OS_CMSIS_RTOS2       3
#define LV_OS_RTTHREAD          4
#define LV_OS_WINDOWS           5
#define LV_OS_CUSTOM            255

#define LV_COORD_MAX            (0x7FFF)
#define LV_COORD_MIN            (-LV_COORD_MAX)
#define LV_SIZE_CONTENT         (LV_COORD_MIN + 1)

#define LV_OPA_TRANSP           0
#define LV_OPA_0                0
#define LV_OPA_10               25
#define LV_OPA_20               51
#define LV_OPA_30               76
#define LV_OPA_40               102
#define LV_OPA_50               127
#define LV_OPA_60               153
#define LV_OPA_70               178
#define LV_OPA_80               204
#define LV_OPA_90               229
#define LV_OPA_100              255
#define LV_OPA_COVER            255

typedef int32_t lv_coord_t;
typedef uint32_t lv_opa_t;
typedef uint32_t lv_state_t;
typedef uint32_t lv_part_t;
typedef uint32_t lv_style_selector_t;

/* Forward declarations */
struct _lv_obj_t;
struct _lv_display_t;
struct _lv_layer_t;
struct _lv_draw_unit_t;
struct _lv_draw_task_t;

#define LV_UNUSED(x)            (void)(x)

#ifdef __cplusplus
}
#endif

#endif /* LV_TYPES_H */
