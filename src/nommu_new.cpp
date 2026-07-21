// SPDX-License-Identifier: MIT

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <new>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

struct allocation_header {
	std::size_t mapping_bytes;
	std::size_t requested_bytes;
};

static void allocation_trace(const char *result, std::size_t bytes, void *memory)
{
	char line[128];
	int saved_errno = errno;
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	int length;

	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line),
		"<6>sf2000-frontend: allocation %s bytes=%lu memory=%p errno=%d\n",
		result, (unsigned long)bytes, memory, saved_errno);
	if (length > 0)
		(void)write(fd, line, (std::size_t)length);
	close(fd);
}

static void *mapping_allocate(std::size_t bytes)
{
	const std::size_t total = bytes + sizeof(allocation_header);
	if (total < bytes) {
		errno = ENOMEM;
		return nullptr;
	}
	const std::size_t mapping_bytes = (total + 4095u) & ~std::size_t(4095u);
	void *mapping = mmap(nullptr, mapping_bytes, PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (mapping != MAP_FAILED) {
		allocation_header *header = static_cast<allocation_header *>(mapping);
		header->mapping_bytes = mapping_bytes;
		header->requested_bytes = bytes;
		return header + 1;
	}
	return nullptr;
}

static void mapping_release(void *memory)
{
	if (memory) {
		allocation_header *header =
			static_cast<allocation_header *>(memory) - 1;
		(void)munmap(header, header->mapping_bytes);
	}
}

extern "C" void *__wrap_malloc(std::size_t bytes)
{
	return mapping_allocate(bytes ? bytes : 1u);
}

extern "C" void __wrap_free(void *memory)
{
	mapping_release(memory);
}

extern "C" void *__wrap_calloc(std::size_t count, std::size_t bytes)
{
	if (bytes && count > (~std::size_t(0) / bytes)) {
		errno = ENOMEM;
		return nullptr;
	}
	return mapping_allocate(count * bytes);
}

extern "C" void *__wrap_realloc(void *memory, std::size_t bytes)
{
	allocation_header *old_header;
	void *replacement;

	if (!memory)
		return __wrap_malloc(bytes);
	if (!bytes) {
		mapping_release(memory);
		return nullptr;
	}
	old_header = static_cast<allocation_header *>(memory) - 1;
	replacement = mapping_allocate(bytes);
	if (!replacement)
		return nullptr;
	std::memcpy(replacement, memory,
		bytes < old_header->requested_bytes ? bytes : old_header->requested_bytes);
	mapping_release(memory);
	return replacement;
}

void *operator new(std::size_t bytes)
{
	if (void *memory = mapping_allocate(bytes))
		return memory;
	allocation_trace("failed", bytes, nullptr);
	throw std::bad_alloc();
}

void *operator new[](std::size_t bytes)
{
	return ::operator new(bytes);
}

void operator delete(void *memory) noexcept
{
	mapping_release(memory);
}

void operator delete[](void *memory) noexcept
{
	::operator delete(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
	::operator delete(memory);
}

void operator delete[](void *memory, std::size_t) noexcept
{
	::operator delete(memory);
}
