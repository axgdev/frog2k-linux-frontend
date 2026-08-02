// SPDX-License-Identifier: MIT

#include "sf2000_log.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#define LOG_FLUSH_REQUEST "/run/sf2000-log-flush-request"
#define LOG_FLUSH_DONE "/run/sf2000-log-flush-done"
#define PERFORMANCE_MARKER "/run/sf2000-performance-active"
#define PERFORMANCE_READY_MARKER "/run/sf2000-performance-ready"

static int write_all(int fd, const char *data, size_t bytes)
{
	while (bytes) {
		ssize_t written = write(fd, data, bytes);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (!written)
			return -1;
		data += written;
		bytes -= (size_t)written;
	}
	return 0;
}

int sf2000_log_flush(const char *reason)
{
	unsigned attempt;
	char result[3];
	int fd;
	ssize_t got;

	(void)unlink(LOG_FLUSH_DONE);
	fd = open(LOG_FLUSH_REQUEST, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
		0644);
	if (fd < 0)
		return -1;
	if (!reason)
		reason = "unspecified";
	if (write_all(fd, reason, strlen(reason)) != 0) {
		close(fd);
		(void)unlink(LOG_FLUSH_REQUEST);
		return -1;
	}
	if (close(fd) != 0) {
		(void)unlink(LOG_FLUSH_REQUEST);
		return -1;
	}
	for (attempt = 0; attempt < 100u; attempt++) {
		fd = open(LOG_FLUSH_DONE, O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			got = read(fd, result, sizeof(result));
			close(fd);
			if (got == (ssize_t)sizeof(result)) {
				(void)unlink(LOG_FLUSH_DONE);
				return !memcmp(result, "ok\n", sizeof(result)) ? 0 : -1;
			}
		}
		(void)poll(NULL, 0, 10);
	}
	return -1;
}

int sf2000_performance_begin(void)
{
	unsigned attempt;
	int fd = open(PERFORMANCE_MARKER,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	if (fd < 0)
		return -1;
	if (close(fd) != 0)
		return -1;
	for (attempt = 0; attempt < 100u; attempt++) {
		if (access(PERFORMANCE_READY_MARKER, F_OK) == 0)
			return 0;
		(void)poll(NULL, 0, 10);
	}
	return -1;
}

void sf2000_performance_end(void)
{
	(void)unlink(PERFORMANCE_MARKER);
}
