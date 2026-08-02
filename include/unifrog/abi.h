/* SPDX-License-Identifier: MIT
 *
 * Linux compatibility declarations for the JS2300 libretro adapter.
 *
 * The original UniFrog/HCRTOS host reserves a top-of-application-memory
 * region for large scripts. Linux owns the process allocator instead, so the
 * adapter deliberately reports that optional reservation as unavailable and
 * lets JS2300 use its normal calloc-backed heap.
 */
#ifndef SF2000_LINUX_UNIFROG_ABI_H
#define SF2000_LINUX_UNIFROG_ABI_H

#include <stddef.h>

static inline int unifrog_abi_application_memory_reserve_top(size_t bytes,
	unsigned alignment, void **out_memory)
{
	(void)bytes;
	(void)alignment;
	if (out_memory)
		*out_memory = NULL;
	return -1;
}

static inline int unifrog_abi_application_memory_release_top(void *memory)
{
	(void)memory;
	return 0;
}

#endif
