/*
 * Copyright (c) 2015 Michal Koutny
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libc
 * @{
 */
/** @file
 *
 * Implementation of dynamic array that grows or shrinks based upon no. of
 * items it contains. Non-negligible part of implementation is in @ref
 * dyn_array.h because of type genericity.
 */

#include <assert.h>
#include <errno.h>
#include <adt/dyn_array.h>
#include <macros.h>
#include <mem.h>
#include <stdlib.h>


static void dyn_array_clear(dyn_array_t *da)
{
	da->size = 0;
}

static int dyn_array_realloc(dyn_array_t *da, size_t capacity)
{
	if (capacity == da->capacity) {
		return EOK;
	}

	void *new_data = realloc(da->_data, da->_item_size * capacity);
	if (new_data) {
		da->_data = new_data;
		da->capacity = capacity;
	}
	return (new_data == NULL) ? ENOMEM : EOK;
}

void dyn_array_destroy(dyn_array_t *da)
{
	dyn_array_clear(da);
	free(da->_data);
	da->capacity = 0;
}

/** Remove item at give position, shift rest of array */
void dyn_array_remove(dyn_array_t *da, size_t index)
{
	assert(index < da->size);
	_dyn_array_unshift(da, index, 1);
	int rc = _dyn_array_reserve(da, da->size);
        assert(rc == EOK);
}

int _dyn_array_initialize(dyn_array_t *da, size_t item_size, size_t capacity)
{
	da->_item_size = item_size;
	da->_data = NULL;
	
	da->capacity = 0;
	da->size = 0;

	return _dyn_array_reserve(da, capacity);
}

void *_dyn_array_get(dyn_array_t *da, size_t index)
{
	assert(index < da->size);
	return da->_data + (index * da->_item_size);
}

/** Grows/shrinks array so that it effeciently stores desired capacity
 *
 * @param      da
 * @param[in]  desired capacity of array
 *
 * @return EOK
 * @return ENOMEM
 */
int _dyn_array_reserve(dyn_array_t *da, size_t capacity)
{
	const size_t factor = 2;
	size_t new_capacity;
	if (capacity > da->capacity) {
		new_capacity = max(da->capacity * factor, capacity);
	} else if (capacity < da->capacity / factor) {
		new_capacity = min(da->capacity / factor, capacity);
	} else {
		new_capacity = capacity;
	}

	return dyn_array_realloc(da, new_capacity);
}

/** Shift block of array
 *
 * Extends size of dynamic array, assumes sufficient capacity.
 *
 * @param      da
 * @param[in]  index   first item shifted
 * @param[in]  offset  shift in no. of items
 */
void _dyn_array_shift(dyn_array_t *da, size_t index, size_t offset)
{
	assert(da->capacity >= da->size + offset);

	void *src = da->_data + index * da->_item_size;
	void *dst = da->_data + (index + offset) * da->_item_size;
	size_t size = (da->size - index) * da->_item_size;
	memmove(dst, src, size);
	da->size += offset;
}

/** Unshift block of array
 *
 * Reduce size of dynamic array
 *
 * @param      da
 * @param[in]  index   first item unshifted (removed)
 * @param[in]  offset  shift in no. of items
 */
void _dyn_array_unshift(dyn_array_t *da, size_t index, size_t offset)
{
	void *src = da->_data + (index + offset) * da->_item_size;
	void *dst = da->_data + index * da->_item_size;
	size_t size = (da->size - index - offset) * da->_item_size;
	memmove(dst, src, size);
	da->size -= offset;
}


/** @}
 */
