/**
 * @file ring_buff.h
 * @author Mauro Medina 
 * @brief Ring-buffer library
 * @version 1.0.1
 * @date 2024-01-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RING_BUFF_H_
#define RING_BUFF_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup doc_driver_hal_utils_ringbuff
 *
 * @{
 */

/**
 * \brief Ring buffer element type
 */
struct ringbuff {
	uint8_t  *buf;           /** Buffer base address */
	uint32_t len;          /** Buffer len */
	uint32_t data_size;     /** Data size */
	uint8_t *p_read;    /** Buffer read index */
	uint8_t *p_write;   /** Buffer write index */
};

/**
 * \brief Ring buffer init
 *
 * \param[in] rb The pointer to a ring buffer structure instance
 * \param[in] buf Space to store the data
 * \param[in] len The buffer length, must be aligned with power of 2
 *
 * \return ERR_NONE on success, or an error code on failure.
 */
int32_t ringbuff_init(struct ringbuff *const rb, void *buf, uint32_t len, uint32_t data_size);

/**
 * \brief Get one byte from ring buffer, the user needs to handle the concurrent
 * access on buffer via put/get/flush
 *
 * \param[in] rb The pointer to a ring buffer structure instance
 * \param[in] data One byte space to store the read data
 *
 * \return ERR_NONE on success, or an error code on failure.
 */
int32_t ringbuff_get(struct ringbuff *const rb, void *data);

/**
 * \brief Put one byte to ring buffer, the user needs to handle the concurrent access
 * on buffer via put/get/flush
 *
 * \param[in] rb The pointer to a ring buffer structure instance
 * \param[in] data One byte data to be put into ring buffer
 *
 * \return ERR_NONE on success, or an error code on failure.
 */
int32_t ringbuff_put(struct ringbuff *const rb, void *data);

/**
 * \brief Return the element number of ring buffer
 *
 * \param[in] rb The pointer to a ring buffer structure instance
 *
 * \return The number of elements in ring buffer [0, rb->size]
 */
uint32_t ringbuff_num(const struct ringbuff *const rb);

/**
 * \brief Flush ring buffer, the user needs to handle the concurrent access on buffer
 * via put/get/flush
 *
 * \param[in] rb The pointer to a ring buffer structure instance
 *
 * \return ERR_NONE on success, or an error code on failure.
 */
uint32_t ringbuff_flush(struct ringbuff *const rb);

/**@}*/

#ifdef __cplusplus
}
#endif
#endif /* RING_BUFF_H_ */