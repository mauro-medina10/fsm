/**
 * @file queue.h
 * @brief Minimal host mock of FreeRTOS static queue API for unit-testing FSM_USE_FREERTOS_API
 */
#ifndef MOCK_FREERTOS_QUEUE_H_
#define MOCK_FREERTOS_QUEUE_H_

#include <string.h>
#include "freertos/FreeRTOS.h"

#define queueSEND_TO_BACK  ((BaseType_t)0)
#define queueSEND_TO_FRONT ((BaseType_t)1)

static inline QueueHandle_t xQueueCreateStatic(UBaseType_t uxQueueLength,
                                               UBaseType_t uxItemSize,
                                               uint8_t *pucQueueStorage,
                                               StaticQueue_t *pxQueueBuffer)
{
    if ((uxQueueLength == 0U) || (uxItemSize == 0U) ||
        (pucQueueStorage == NULL) || (pxQueueBuffer == NULL))
    {
        return NULL;
    }

    pxQueueBuffer->buf       = pucQueueStorage;
    pxQueueBuffer->capacity  = uxQueueLength;
    pxQueueBuffer->item_size = uxItemSize;
    pxQueueBuffer->head      = 0U;
    pxQueueBuffer->tail      = 0U;
    pxQueueBuffer->count     = 0U;
    pxQueueBuffer->deleted   = 0;
    return pxQueueBuffer;
}

static inline void vQueueDelete(QueueHandle_t q)
{
    if (q != NULL) {
        q->count   = 0U;
        q->head    = 0U;
        q->tail    = 0U;
        q->deleted = 1;
    }
}

static inline BaseType_t xQueueGenericSend(QueueHandle_t q,
                                           const void *pvItemToQueue,
                                           TickType_t xTicksToWait,
                                           BaseType_t xCopyPosition)
{
    (void)xTicksToWait;
    if ((q == NULL) || (pvItemToQueue == NULL) || q->deleted || (q->count >= q->capacity)) {
        return pdFALSE;
    }

    if (xCopyPosition == queueSEND_TO_FRONT) {
        q->head = (q->head == 0U) ? (q->capacity - 1U) : (q->head - 1U);
        memcpy(q->buf + ((size_t)q->head * (size_t)q->item_size), pvItemToQueue, q->item_size);
    } else {
        memcpy(q->buf + ((size_t)q->tail * (size_t)q->item_size), pvItemToQueue, q->item_size);
        q->tail = (q->tail + 1U) % q->capacity;
    }
    q->count++;
    return pdTRUE;
}

static inline BaseType_t xQueueGenericSendFromISR(QueueHandle_t q,
                                                  const void *pvItemToQueue,
                                                  BaseType_t *pxHigherPriorityTaskWoken,
                                                  BaseType_t xCopyPosition)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xQueueGenericSend(q, pvItemToQueue, 0U, xCopyPosition);
}

static inline BaseType_t xQueueSend(QueueHandle_t q,
                                    const void *pvItemToQueue,
                                    TickType_t xTicksToWait)
{
    return xQueueGenericSend(q, pvItemToQueue, xTicksToWait, queueSEND_TO_BACK);
}

static inline BaseType_t xQueueSendFromISR(QueueHandle_t q,
                                           const void *pvItemToQueue,
                                           BaseType_t *pxHigherPriorityTaskWoken)
{
    return xQueueGenericSendFromISR(q, pvItemToQueue, pxHigherPriorityTaskWoken, queueSEND_TO_BACK);
}

static inline BaseType_t xQueueReceive(QueueHandle_t q,
                                       void *pvBuffer,
                                       TickType_t xTicksToWait)
{
    (void)xTicksToWait;
    if ((q == NULL) || (pvBuffer == NULL) || q->deleted || (q->count == 0U)) {
        return pdFALSE;
    }

    memcpy(pvBuffer, q->buf + ((size_t)q->head * (size_t)q->item_size), q->item_size);
    q->head = (q->head + 1U) % q->capacity;
    q->count--;
    return pdTRUE;
}

static inline BaseType_t xQueueReceiveFromISR(QueueHandle_t q,
                                              void *pvBuffer,
                                              BaseType_t *pxHigherPriorityTaskWoken)
{
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xQueueReceive(q, pvBuffer, 0U);
}

static inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q)
{
    if ((q == NULL) || q->deleted) {
        return 0U;
    }
    return q->count;
}

static inline UBaseType_t uxQueueMessagesWaitingFromISR(QueueHandle_t q)
{
    return uxQueueMessagesWaiting(q);
}

static inline BaseType_t xQueueReset(QueueHandle_t q)
{
    if ((q == NULL) || q->deleted) {
        return pdFALSE;
    }
    q->head  = 0U;
    q->tail  = 0U;
    q->count = 0U;
    return pdPASS;
}

#endif /* MOCK_FREERTOS_QUEUE_H_ */
