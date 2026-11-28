/**
 * @file FreeRTOS.h
 * @brief Minimal host mock of FreeRTOS types for unit-testing FSM_USE_FREERTOS_API
 */
#ifndef MOCK_FREERTOS_H_
#define MOCK_FREERTOS_H_

#include <stddef.h>
#include <stdint.h>

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;

#define pdFALSE ((BaseType_t)0)
#define pdTRUE  ((BaseType_t)1)
#define pdPASS  (pdTRUE)

extern int mock_freertos_in_isr;

static inline BaseType_t xPortInIsrContext(void)
{
    return mock_freertos_in_isr ? pdTRUE : pdFALSE;
}

typedef struct {
    uint8_t *buf;
    UBaseType_t capacity;
    UBaseType_t item_size;
    UBaseType_t head;
    UBaseType_t tail;
    UBaseType_t count;
    int deleted;
} StaticQueue_t;

typedef StaticQueue_t *QueueHandle_t;

#endif /* MOCK_FREERTOS_H_ */
