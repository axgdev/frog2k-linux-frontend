// SPDX-License-Identifier: MIT
#include "libretro_min.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int sf2000_load_content(const char *path, struct retro_game_info *game)
{
	off_t length;
	size_t done = 0;
	unsigned char *data;
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return -1;
	length = lseek(fd, 0, SEEK_END);
	if (length <= 0 || length > 16 * 1024 * 1024 ||
			lseek(fd, 0, SEEK_SET) < 0) {
		close(fd);
		errno = EINVAL;
		return -1;
	}
	data = malloc((size_t)length);
	if (!data) {
		close(fd);
		return -1;
	}
	while (done < (size_t)length) {
		ssize_t got = read(fd, data + done, (size_t)length - done);
		if (got <= 0) {
			free(data);
			close(fd);
			if (!got)
				errno = EIO;
			return -1;
		}
		done += (size_t)got;
	}
	close(fd);
	game->path = path;
	game->data = data;
	game->size = done;
	return 0;
}
