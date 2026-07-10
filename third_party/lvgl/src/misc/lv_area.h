/*
 * Minimal LVGL v9 area/point definitions.
 */
#ifndef LV_AREA_H
#define LV_AREA_H

#include "lv_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_coord_t x1;
    lv_coord_t y1;
    lv_coord_t x2;
    lv_coord_t y2;
} lv_area_t;

typedef struct {
    lv_coord_t x;
    lv_coord_t y;
} lv_point_t;

static inline lv_coord_t lv_area_get_width(const lv_area_t *a)
{
    return a->x2 - a->x1 + 1;
}

static inline lv_coord_t lv_area_get_height(const lv_area_t *a)
{
    return a->y2 - a->y1 + 1;
}

static inline void lv_area_set(lv_area_t *a, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2)
{
    a->x1 = x1;
    a->y1 = y1;
    a->x2 = x2;
    a->y2 = y2;
}

static inline void lv_area_set_width(lv_area_t *a, lv_coord_t w)
{
    a->x2 = a->x1 + w - 1;
}

static inline void lv_area_set_height(lv_area_t *a, lv_coord_t h)
{
    a->y2 = a->y1 + h - 1;
}

#define LV_MAX(a, b)            ((a) > (b) ? (a) : (b))
#define LV_MIN(a, b)            ((a) < (b) ? (a) : (b))
#define LV_CLAMP(v, min, max)   ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))

static inline bool lv_area_is_in(const lv_area_t *ain, const lv_area_t *aout, lv_coord_t r)
{
    return (ain->x1 >= aout->x1 - r && ain->y1 >= aout->y1 - r &&
            ain->x2 <= aout->x2 + r && ain->y2 <= aout->y2 + r);
}

static inline bool lv_area_intersect(lv_area_t *res, const lv_area_t *a1, const lv_area_t *a2)
{
    res->x1 = LV_MAX(a1->x1, a2->x1);
    res->y1 = LV_MAX(a1->y1, a2->y1);
    res->x2 = LV_MIN(a1->x2, a2->x2);
    res->y2 = LV_MIN(a1->y2, a2->y2);
    return (res->x1 <= res->x2 && res->y1 <= res->y2);
}

static inline void lv_area_join(lv_area_t *aout, const lv_area_t *a1, const lv_area_t *a2)
{
    aout->x1 = LV_MIN(a1->x1, a2->x1);
    aout->y1 = LV_MIN(a1->y1, a2->y1);
    aout->x2 = LV_MAX(a1->x2, a2->x2);
    aout->y2 = LV_MAX(a1->y2, a2->y2);
}

#ifdef __cplusplus
}
#endif

#endif /* LV_AREA_H */
