/**
 * @file ring_buff.h
 * @author Mauro Medina
 * @brief Generic fixed-element-size ring buffer library
 * @version 1.1.0
 * @date 2024-01-19
 *
 * @copyright Copyright (c) 2024
 */

#ifndef RING_BUFF_H_
#define RING_BUFF_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ring buffer control structure.
 *
 * Uses one sentinel slot to distinguish full from empty, so usable capacity
 * is (len - 1) elements.
 */
struct ringbuff {
	uint8_t *buf;        /**< Buffer base address */
	uint32_t len;        /**< Total number of slots (must be >= 2) */
	uint32_t data_size;  /**< Size of each element in bytes */
	uint8_t *p_read;     /**< Buffer read pointer */
	uint8_t *p_write;    /**< Buffer write pointer */
};

/**
 * @brief Initialize a ring buffer.
 *
 * @param[in] rb        Pointer to ring buffer instance
 * @param[in] buf       Backing storage of at least (len * data_size) bytes
 * @param[in] len       Number of element slots (must be >= 2; usable capacity is len - 1)
 * @param[in] data_size Size of each element in bytes (must be > 0)
 *
 * @return 0 on success, -1 on invalid arguments.
 */
int32_t ringbuff_init(struct ringbuff *const rb, void *buf, uint32_t len, uint32_t data_size);

/**
 * @brief Pop one element from the front of the ring buffer.
 *
 * The caller is responsible for synchronizing concurrent access across
 * put/get/flush calls (e.g., ISR/task critical sections).
 *
 * @param[in]  rb   Pointer to ring buffer instance
 * @param[out] data Destination buffer of at least rb->data_size bytes
 *
 * @return 0 on success, -1 if empty or invalid arguments.
 */
int32_t ringbuff_get(struct ringbuff *const rb, void *data);

/**
 * @brief Push one element to the back of the ring buffer.
 *
 * If the buffer is full, the oldest element is overwritten.
 * The caller is responsible for synchronizing concurrent access.
 *
 * @param[in] rb   Pointer to ring buffer instance
 * @param[in] data Pointer to element to copy into the ring buffer
 *
 * @return 0 on success, -1 on invalid arguments.
 */
int32_t ringbuff_put(struct ringbuff *const rb, const void *data);

/**
 * @brief Push one element to the front (priority position) of the ring buffer.
 *
 * If the buffer is full, the newest (tail) element is dropped to preserve
 * queue integrity. The caller is responsible for synchronizing concurrent access.
 *
 * @param[in] rb   Pointer to ring buffer instance
 * @param[in] data Pointer to element to copy into the front of the ring buffer
 *
 * @return 0 on success, -1 on invalid arguments.
 */
int32_t ringbuff_put_first(struct ringbuff *const rb, const void *data);

/**
 * @brief Return the number of elements currently stored in the ring buffer.
 *
 * @param[in] rb Pointer to ring buffer instance
 *
 * @return Number of elements in [0, rb->len - 1], or 0 if invalid.
 */
uint32_t ringbuff_num(const struct ringbuff *const rb);

/**
 * @brief Discard all elements currently in the ring buffer.
 *
 * The caller is responsible for synchronizing concurrent access.
 *
 * @param[in] rb Pointer to ring buffer instance
 *
 * @return 0 on success, (uint32_t)-1 on invalid arguments.
 */
uint32_t ringbuff_flush(struct ringbuff *const rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFF_H_ */
