// SPDX-License-Identifier: MIT

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" void *__wrap_malloc(std::size_t);
extern "C" void *__wrap_calloc(std::size_t, std::size_t);
extern "C" void *__wrap_realloc(void *, std::size_t);
extern "C" void __wrap_free(void *);

int main()
{
	void *first = __wrap_malloc(1024);

	assert(first);
	assert((reinterpret_cast<std::uintptr_t>(first) & 15u) == 0);
	__wrap_free(first);
	for (unsigned i = 0; i < 4096; ++i) {
		void *transient = __wrap_malloc(1024);

		assert(transient);
		assert((reinterpret_cast<std::uintptr_t>(transient) & 15u) == 0);
		std::memset(transient, (int)i, 1024);
		__wrap_free(transient);
	}

	void *live[64];
	for (unsigned i = 0; i < 64; ++i) {
		live[i] = __wrap_calloc(1, 1024 + i);
		assert(live[i]);
		for (unsigned j = 0; j < 1024 + i; ++j)
			assert(static_cast<unsigned char *>(live[i])[j] == 0);
	}
	for (unsigned i = 0; i < 64; i += 2)
		__wrap_free(live[i]);
	for (unsigned i = 1; i < 64; i += 2)
		__wrap_free(live[i]);

	void *coalesced = __wrap_malloc(60u * 1024u);
	assert(coalesced);
	std::memset(coalesced, 0x5a, 4096);
	void *grown = __wrap_realloc(coalesced, 96u * 1024u);
	assert(grown);
	for (unsigned i = 0; i < 4096; ++i)
		assert(static_cast<unsigned char *>(grown)[i] == 0x5a);
	__wrap_free(grown);

	return 0;
}
