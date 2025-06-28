/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Iterator for I/O, modelled on Linux but significantly simplified
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#ifndef __IOVEC_H
#define __IOVEC_H

#include <linux/types.h>

/**
 * enum iov_type_t - defines the type of this I/O vector
 *
 * @OIV_BUF: Simple buffer with a size
 */
enum iov_type_t {
	OIV_BUF,
};

/**
 * struct iov_iter - holds an interator for I/O
 *
 * Do not use this directly. Instead, call copy_to_iter()
 *
 * Note that this is much simpler than the Linux version, but can be extended
 * later as needed. It is introduced so that we can use the desired API for
 * read(), etc. It is also commented properly.
 *
 * @type: Type of the iterator (always OIV_BUF)
 * @data_source: true if this iterator produces data, false if it consumes it
 * @offset: Current offset within the buffer
 * @buf: Contiguous data buffer to use (other things could be added to the
 *	union later)
 * @count: Size of data buffer
 */
struct iov_iter {
	enum iov_type_t type;
	bool data_source;
	ssize_t offset;
	union {
		void *ubuf;
	};
	ssize_t count;
};

/**
 * iter_iov_ptr() - Get a pointer to the current position
 *
 * @iter: Iterator to examine
 * Return: pointer to the start of the buffer portion to read/write
 */
static inline void *iter_iov_ptr(const struct iov_iter *iter)
{
	return iter->ubuf + iter->offset;
};

/**
 * iter_iov_avail() - Get the number of bytes available at the current postiion
 *
 * @iter: iterator to examine
 * Return: number of bytes which can be read/written
 */
static inline ssize_t iter_iov_avail(const struct iov_iter *iter)
{
	return iter->count - iter->offset;
};

static inline void iter_ubuf(struct iov_iter *iter, bool data_source, void *buf,
			     size_t count)
{
	*iter = (struct iov_iter) {
		.data_source = data_source,
		.ubuf = buf,
		.count = count,
	};
}

static inline void iter_advance(struct iov_iter *iter, size_t len)
{
	iter->offset += len;
}

#endif
