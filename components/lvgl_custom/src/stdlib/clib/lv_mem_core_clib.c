/**
 * @file lv_mem_core_clib.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_mem.h"
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CLIB
#include "../../stdlib/lv_mem.h"
#include <stdlib.h>
#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/
/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_mem_init(void) { return; /*Nothing to init*/ }

void lv_mem_deinit(void) { return; /*Nothing to deinit*/ }

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) {
    /*Not supported*/
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) {
    /*Not supported*/
    LV_UNUSED(pool);
    return;
}

void *lv_malloc_core(size_t size) {
#if defined(ESP_PLATFORM)
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (p == NULL)
        p = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    return p;
#else
    return malloc(size);
#endif
}

void *lv_realloc_core(void *p, size_t new_size) {
#if defined(ESP_PLATFORM)
    void *np = heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM);
    if (np == NULL)
        np = heap_caps_realloc(p, new_size, MALLOC_CAP_DEFAULT);
    return np;
#else
    return realloc(p, new_size);
#endif
}

void lv_free_core(void *p) {
#if defined(ESP_PLATFORM)
    heap_caps_free(p);
#else
    free(p);
#endif
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p) {
    /*Not supported*/
    LV_UNUSED(mon_p);
    return;
}

lv_result_t lv_mem_test_core(void) {
    /*Not supported*/
    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_STDLIB_CLIB*/
