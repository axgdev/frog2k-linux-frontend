// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#define JS2300_FS_HANDLES 4
#define JS2300_FS_RECORD_BYTES 0x428u
#define JS2300_FS_NAME_OFFSET 0x22u

static DIR *handles[JS2300_FS_HANDLES];

int fs_opendir(const char *path)
{
	unsigned i;
	DIR *directory;

	if (!path)
		return -1;
	directory = opendir(path);
	if (!directory)
		return -1;
	for (i = 0; i < JS2300_FS_HANDLES; i++) {
		if (!handles[i]) {
			handles[i] = directory;
			return (int)i + 1;
		}
	}
	closedir(directory);
	errno = EMFILE;
	return -1;
}

int fs_closedir(int fd)
{
	if (fd <= 0 || fd > JS2300_FS_HANDLES || !handles[fd - 1]) {
		errno = EBADF;
		return -1;
	}
	closedir(handles[fd - 1]);
	handles[fd - 1] = NULL;
	return 0;
}

ssize_t fs_readdir(int fd, void *buffer)
{
	struct dirent *entry;
	const char *name;
	size_t length;

	if (fd <= 0 || fd > JS2300_FS_HANDLES || !handles[fd - 1] || !buffer) {
		errno = EBADF;
		return -1;
	}
	entry = readdir(handles[fd - 1]);
	if (!entry)
		return -1;
	memset(buffer, 0, JS2300_FS_RECORD_BYTES);
	name = entry->d_name;
	length = strlen(name);
	if (length >= JS2300_FS_RECORD_BYTES - JS2300_FS_NAME_OFFSET)
		length = JS2300_FS_RECORD_BYTES - JS2300_FS_NAME_OFFSET - 1u;
	memcpy((unsigned char *)buffer + JS2300_FS_NAME_OFFSET, name, length);
	return (ssize_t)(JS2300_FS_NAME_OFFSET + length + 1u);
}
