/**
 * @file ring_buff.c
 * @author Mauro Medina
 * @brief Generic fixed-element-size ring buffer library
 * @version 1.1.0
 * @date 2024-01-19
 *
 * @copyright Copyright (c) 2024
 */

#include <stddef.h>
#include <string.h>

#include "ring_buff.h"

static inline uint8_t *ringbuff_end(const struct ringbuff *const rb)
{
	return rb->buf + ((size_t)rb->data_size * (size_t)rb->len);
}

static inline uint8_t *ringbuff_step_forward(const struct ringbuff *const rb, uint8_t *ptr)
{
	ptr += rb->data_size;
	if (ptr >= ringbuff_end(rb)) {
		ptr = rb->buf;
	}
	return ptr;
}

static inline uint8_t *ringbuff_step_backward(const struct ringbuff *const rb, uint8_t *ptr)
{
	if (ptr == rb->buf) {
		return rb->buf + ((size_t)rb->data_size * (size_t)(rb->len - 1U));
	}
	return ptr - rb->data_size;
}

int32_t ringbuff_init(struct ringbuff *const rb, void *buf, uint32_t len, uint32_t data_size)
{
	if ((rb == NULL) || (buf == NULL) || (len < 2U) || (data_size == 0U)) {
		return -1;
	}

	rb->buf       = (uint8_t *)buf;
	rb->len       = len;
	rb->data_size = data_size;
	rb->p_read    = rb->buf;
	rb->p_write   = rb->buf;

	return 0;
}

int32_t ringbuff_get(struct ringbuff *const rb, void *data)
{
	if ((rb == NULL) || (data == NULL) || (rb->buf == NULL)) {
		return -1;
	}

	if (rb->p_write != rb->p_read) {
		memcpy(data, rb->p_read, rb->data_size);
		rb->p_read = ringbuff_step_forward(rb, rb->p_read);
		return 0;
	}

	return -1;
}

int32_t ringbuff_put(struct ringbuff *const rb, const void *data)
{
	if ((rb == NULL) || (data == NULL) || (rb->buf == NULL)) {
		return -1;
	}

	memcpy(rb->p_write, data, rb->data_size);
	rb->p_write = ringbuff_step_forward(rb, rb->p_write);

	/* Buffer full strategy: overwrite the oldest element. */
	if (rb->p_write == rb->p_read) {
		rb->p_read = ringbuff_step_forward(rb, rb->p_read);
	}

	return 0;
}

int32_t ringbuff_put_first(struct ringbuff *const rb, const void *data)
{
	if ((rb == NULL) || (data == NULL) || (rb->buf == NULL)) {
		return -1;
	}

	uint8_t *next_read = ringbuff_step_backward(rb, rb->p_read);

	/* If buffer is full, drop the tail element so p_read != p_write is preserved. */
	if (next_read == rb->p_write) {
		rb->p_write = ringbuff_step_backward(rb, rb->p_write);
	}

	rb->p_read = next_read;
	memcpy(rb->p_read, data, rb->data_size);

	return 0;
}

uint32_t ringbuff_num(const struct ringbuff *const rb)
{
	if ((rb == NULL) || (rb->buf == NULL) || (rb->data_size == 0U)) {
		return 0U;
	}

	ptrdiff_t diff = rb->p_write - rb->p_read;
	ptrdiff_t count = diff / (ptrdiff_t)rb->data_size;

	if (count < 0) {
		count += (ptrdiff_t)rb->len;
	}

	return (uint32_t)count;
}

uint32_t ringbuff_flush(struct ringbuff *const rb)
{
	if ((rb == NULL) || (rb->buf == NULL)) {
		return (uint32_t)-1;
	}

	rb->p_read = rb->p_write;

	return 0U;
}
