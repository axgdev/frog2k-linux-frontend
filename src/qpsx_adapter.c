#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* The QPSX code was also used by the HCRTOS build, where these small OS
 * services are supplied by the firmware.  The Linux frontend keeps the core
 * independent from that ABI and maps the services to POSIX here. */

static const char *qpsx_path(const char *path)
{
	static char translated[512];
	const char *firmware_root = "/mnt/sda1";
	const char *linux_root = "/mnt/sd";
	size_t root_length = strlen(firmware_root);

	if (strncmp(path, firmware_root, root_length) != 0 ||
		(path[root_length] != '\0' && path[root_length] != '/'))
		return path;

	snprintf(translated, sizeof(translated), "%s%s", linux_root,
		path + root_length);
	return translated;
}

void xlog(const char *format, ...)
{
	/* QPSX's firmware diagnostics are routed to the kernel log so the
	 * device journal can show exactly where a core load or run stalls.
	 * The core gates every xlog() call behind its own debug-log switch
	 * (g_debug_log_enabled, forced on by the temporary qpsx-debug.patch
	 * while the on-device retro_load_game stall is diagnosed), so this
	 * stays silent in normal use and only appears while tracing.
	 *
	 * The kmsg device rate-limits per open fd, so like the frontend's
	 * log_kmsg() each message opens, writes and closes its own fd. */
	va_list args;
	char line[320];
	int length;
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd < 0)
		return;
	va_start(args, format);
	length = vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	if (length > 0 && (size_t)length + 3u < sizeof(line)) {
		memmove(line + 3, line, (size_t)length + 1u);
		line[0] = '<';
		line[1] = '6';
		line[2] = '>';
		(void)write(fd, line, (size_t)length + 3u);
	}
	close(fd);
}

void xlog_clear(void)
{
}

uint32_t os_get_tick_count(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 0;
	return (uint32_t)(now.tv_sec * 1000u + (unsigned)now.tv_nsec / 1000000u);
}

enum {
	QPSX_FS_O_WRONLY = 0x0001,
	QPSX_FS_O_RDWR = 0x0002,
	QPSX_FS_O_CREAT = 0x0100,
	QPSX_FS_O_TRUNC = 0x0200,
};

int fs_open(const char *path, int flags, int permissions)
{
	int posix_flags = O_RDONLY;

	if ((flags & QPSX_FS_O_RDWR) == QPSX_FS_O_RDWR)
		posix_flags = O_RDWR;
	else if (flags & QPSX_FS_O_WRONLY)
		posix_flags = O_WRONLY;
	if (flags & QPSX_FS_O_CREAT)
		posix_flags |= O_CREAT;
	if (flags & QPSX_FS_O_TRUNC)
		posix_flags |= O_TRUNC;
	return open(qpsx_path(path), posix_flags, (mode_t)permissions);
}

ssize_t fs_read(int fd, void *buffer, size_t bytes)
{
	return read(fd, buffer, bytes);
}

ssize_t fs_write(int fd, const void *buffer, size_t bytes)
{
	return write(fd, buffer, bytes);
}

int fs_close(int fd)
{
	return close(fd);
}

int64_t fs_lseek(int fd, int64_t offset, int whence)
{
	return (int64_t)lseek(fd, (off_t)offset, whence);
}

int fs_sync(const char *path)
{
	int fd = open(qpsx_path(path), O_RDONLY);
	int result = 0;

	if (fd >= 0) {
		result = fsync(fd);
		close(fd);
	}
	return result;
}

#define QPSX_DIRECTORY_SLOTS 16
static DIR *qpsx_directories[QPSX_DIRECTORY_SLOTS];

int fs_opendir(const char *path)
{
	unsigned index;
	DIR *directory = opendir(qpsx_path(path));

	if (!directory)
		return -1;
	for (index = 0; index < QPSX_DIRECTORY_SLOTS; index++) {
		if (!qpsx_directories[index]) {
			qpsx_directories[index] = directory;
			return (int)index + 1;
		}
	}
	closedir(directory);
	return -1;
}

int fs_readdir(int handle, void *buffer)
{
	DIR *directory;
	struct dirent *next;
	struct qpsx_dirent {
		uint32_t d_ino;
		char d_name[256];
	} *entry = buffer;
	unsigned index;

	if (handle <= 0 || handle > QPSX_DIRECTORY_SLOTS)
		return -1;
	index = (unsigned)handle - 1;
	directory = qpsx_directories[index];
	if (!directory)
		return -1;
	next = readdir(directory);
	if (!next)
		return 0;
	entry->d_ino = (uint32_t)next->d_ino;
	strncpy(entry->d_name, next->d_name, sizeof(entry->d_name) - 1u);
	entry->d_name[sizeof(entry->d_name) - 1u] = '\0';
	return 1;
}

int fs_closedir(int handle)
{
	unsigned index;
	int result;

	if (handle <= 0 || handle > QPSX_DIRECTORY_SLOTS)
		return -1;
	index = (unsigned)handle - 1;
	if (!qpsx_directories[index])
		return -1;
	result = closedir(qpsx_directories[index]);
	qpsx_directories[index] = NULL;
	return result;
}

int fs_mkdir(const char *path, int mode)
{
	return mkdir(qpsx_path(path), (mode_t)mode);
}
