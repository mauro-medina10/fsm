/**
 * @file ring_buff.c
 * @author Mauro Medina 
 * @brief Ring-buffer library
 * @version 1.0.1
 * @date 2024-01-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <stdio.h>
#include <string.h>

#include "ring_buff.h"

/**
 * \brief Ringbuffer init
 */
int32_t ringbuff_init(struct ringbuff *const rb, void *buf, uint32_t len, uint32_t data_size)
{
	assert(rb && buf && len);

	/* len - 1 is faster in calculation */
	rb->len         = len;
	rb->data_size   = data_size;
	rb->p_read      = (uint8_t *)buf;
	rb->p_write     = rb->p_read;
	rb->buf         = (uint8_t *)buf;

	return 0;
}

/**
 * \brief Get one byte from ringbuff
 *
 */
int32_t ringbuff_get(struct ringbuff *const rb, void *data)
{
	assert(rb && data);

	if (rb->p_write != rb->p_read) {
        memcpy(data, (void*)rb->p_read, rb->data_size);
		rb->p_read += rb->data_size;
        if(rb->p_read >= ((uint8_t *)rb->buf + rb->data_size * rb->len))
        {
            rb->p_read = (uint8_t *)rb->buf;
        }
		return 0;
	}

	return -1;
}

/**
 * \brief Put one byte to ringbuff
 *
 */
int32_t ringbuff_put(struct ringbuff *const rb, void *data)
{
	assert(rb);

    memcpy(rb->p_write, data, rb->data_size);

	rb->p_write += rb->data_size;
	/*
	 * buffer full strategy: new data will overwrite the oldest data in
	 * the buffer
	 */
	if (rb->p_write >= ((uint8_t *)rb->buf + rb->data_size * rb->len)) {
		rb->p_write = (uint8_t *)rb->buf;
	}
    if(rb->p_write == rb->p_read)
    {
        rb->p_read += rb->data_size;
		if(rb->p_read >= ((uint8_t *)rb->buf + rb->data_size * rb->len))
		{
			rb->p_read = (uint8_t *)rb->buf;
		}
    }
	return 0;
}

/**
 * \brief Return the element number of ringbuff
 */
uint32_t ringbuff_num(const struct ringbuff *const rb)
{
	assert(rb);

	int32_t len = (int32_t)((rb->p_write - rb->p_read) / rb->data_size);

	if(len < 0)
	{
		len = rb->len - len;
	}

	return len;
}

/**
 * \brief Flush ringbuff
 */
uint32_t ringbuff_flush(struct ringbuff *const rb)
{
	assert(rb);

	rb->p_read = rb->p_write;

	return 0;
}
