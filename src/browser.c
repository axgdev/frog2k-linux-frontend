// SPDX-License-Identifier: MIT

#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "ge_api.h"
#include "sf2000_browser_ui.h"
#include "sf2000_log.h"

extern long syscall(long number, ...);

#define READY_MARKER "/run/sf2000-frontend-ready"
#define RESET_MARKER "/run/sf2000-reboot-request"
#define SHUTDOWN_MARKER "/run/sf2000-shutdown-request"
#define GAMBATTE_PATH "/mnt/sd/sf2000/cores/sf2000-gambatte"
#define GPSP_PATH "/mnt/sd/sf2000/cores/sf2000-gpsp"
#define FCEUMM_PATH "/mnt/sd/sf2000/cores/sf2000-fceumm"
#define PLAYER_PATH "/usr/bin/sf2000-player"
#define SD_ROOT "/mnt/sd"
#define BROWSER_STARTUP_CONFIG_PATH SD_ROOT "/cores/config/browser_startup.cfg"
#define STORAGE_ROOTS_PATH "/run/sf2000-storage-roots"
#define BROWSER_STATE_PATH "/run/sf2000-browser-state"
#define MAX_ENTRIES 128
#define MAX_EXTRA_ROOTS 8u
#define MAX_NAME 128
#define MAX_PATH 512
#define MENU_JUMP 5u
#define MAX_FRAME_PIXELS (320u * 240u)
#define LINUX_DT_DIR 4u
#define LINUX_DT_REG 8u
#define UI_DIAGNOSTIC_PATH SD_ROOT "/sf2000/ui-diagnostic.bin"
#define UI_DIAGNOSTIC_MAGIC "SF2KUID1"
#define BUSYBOX_PATH "/bin/busybox"
#define ZIP_WORK_DIR "/tmp/sf2000-rom"

struct linux_dirent64 {
	uint64_t inode;
	int64_t offset;
	unsigned short record_length;
	unsigned char type;
	char name[];
};

static ssize_t kernel_getdents64(int fd, void *buffer, size_t bytes)
{
#ifdef __mips__
	register long v0 __asm__("$2") = __NR_getdents64;
	register long a0 __asm__("$4") = fd;
	register long a1 __asm__("$5") = (long)buffer;
	register long a2 __asm__("$6") = (long)bytes;
	register long a3 __asm__("$7") = 0;

	__asm__ volatile ("syscall"
		: "+r"(v0), "+r"(a3)
		: "r"(a0), "r"(a1), "r"(a2)
		: "memory");
	if (a3) {
		errno = (int)-v0;
		return -1;
	}
	return v0;
#else
	return syscall(__NR_getdents64, fd, buffer, bytes);
#endif
}

struct entry {
	char name[MAX_NAME];
	unsigned directory;
	/* If non-empty, opening this directory entry jumps here. */
	char target[MAX_PATH];
};
static struct entry entries[MAX_ENTRIES];
static unsigned entry_count, selected, first;
/* Render into normal cached RAM, then publish that immutable image through
 * the GE.  The fbdev mapping is also the GE destination; using it as the UI
 * renderer's working buffer lets CPU stores and a later GE write share cache
 * ownership in a way the small target cannot reliably make coherent. */
static uint16_t ui_pixels[MAX_FRAME_PIXELS];
static uint16_t diagnostic_ge_scanout[MAX_FRAME_PIXELS];
static uint16_t diagnostic_scanout[MAX_FRAME_PIXELS];
static int framebuffer_fd = -1;
static uint32_t framebuffer_phys;
static hcge_context ge_storage;
static hcge_context *ge;
static uint16_t *ge_source;
static uint32_t ge_source_phys;
static uint32_t ge_source_handle;
static unsigned width, height, stride;
static char current[MAX_PATH] = SD_ROOT;
static char restore_entry[MAX_NAME];
static int browser_state_valid;
static char primary_root[MAX_PATH] = SD_ROOT;
static char extra_roots[MAX_EXTRA_ROOTS][MAX_PATH];
static char extra_labels[MAX_EXTRA_ROOTS][MAX_NAME];
static unsigned extra_root_count;
static struct sf2000_ui ui;
enum browser_view { VIEW_HOME, VIEW_LIBRARY, VIEW_SETTINGS, VIEW_DEBUG };
static enum browser_view view = VIEW_HOME;
static unsigned debug_scroll = 0;
static unsigned framebuffer_writes;
static unsigned diagnostic_chord_latched;
static unsigned diagnostic_held;
static unsigned log_flush_chord_latched;
static unsigned log_flush_held;
static void log_message(const char *message);
static int write_frame(void);
static void scan_directory(void);
static void launch_selected(int input);

static void save_browser_state(void)
{
	char state[MAX_PATH + MAX_NAME + 2u];
	int length;
	int fd;

	if (selected >= entry_count)
		return;
	length = snprintf(state, sizeof(state), "%s\n%s\n", current,
		entries[selected].name);
	if (length <= 0 || (size_t)length >= sizeof(state))
		return;
	fd = open(BROWSER_STATE_PATH,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd >= 0) {
		if (write(fd, state, (size_t)length) != length)
			log_message("browser state write failed");
		close(fd);
	}
}

static int browser_state_path_allowed(const char *path)
{
	size_t root_length = strlen(SD_ROOT);

	return !strncmp(path, SD_ROOT, root_length) &&
		(path[root_length] == 0 || path[root_length] == '/');
}

static void load_browser_state(void)
{
	char state[MAX_PATH + MAX_NAME + 2u];
	char *separator;
	char *name;
	size_t state_length;
	size_t name_length;
	ssize_t bytes;
	int fd = open(BROWSER_STATE_PATH, O_RDONLY | O_CLOEXEC);

	browser_state_valid = 0;
	restore_entry[0] = 0;
	if (fd < 0)
		return;
	bytes = read(fd, state, sizeof(state) - 1u);
	close(fd);
	if (bytes <= 0)
		return;
	state[bytes] = 0;
	separator = strchr(state, '\n');
	if (!separator)
		return;
	*separator = 0;
	name = separator + 1;
	separator = strchr(name, '\n');
	if (separator)
		*separator = 0;
	state_length = strlen(state);
	name_length = strlen(name);
	if (!state[0] || !name[0] || !browser_state_path_allowed(state) ||
		state_length >= sizeof(current) || name_length >= sizeof(restore_entry))
		return;
	memcpy(current, state, state_length + 1u);
	memcpy(restore_entry, name, name_length + 1u);
	browser_state_valid = 1;
}

static void restore_browser_selection(void)
{
	unsigned i;

	if (!browser_state_valid || !restore_entry[0])
		return;
	for (i = 0; i < entry_count; i++)
		if (!strcmp(entries[i].name, restore_entry)) {
			selected = i;
			first = 0;
			return;
		}
}

static unsigned menu_move(unsigned value, unsigned count, int direction)
{
	unsigned jump;

	if (!count)
		return 0;
	jump = MENU_JUMP % count;
	if (!jump)
		jump = count - 1u;
	if (direction < 0)
		return (value + count - jump) % count;
	return (value + jump) % count;
}

struct ui_diagnostic_header {
	char magic[8];
	uint32_t version;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t framebuffer_phys;
	uint32_t ge_source_phys;
	uint32_t source_hash;
	uint32_t ge_hash;
	uint32_t cpu_hash;
	uint32_t ge_mismatches;
	uint32_t cpu_mismatches;
	uint32_t frame_bytes;
};

_Static_assert(sizeof(struct ui_diagnostic_header) == 56,
	"unexpected UI diagnostic header size");

static void close_ge_presenter(void)
{
	if (ge && ge_source_handle)
		(void)hcge_linux_free_buffer(ge, ge_source_handle);
	ge_source = NULL;
	ge_source_handle = 0;
	if (ge) {
		hcge_close_context(ge);
		ge = NULL;
	}
}

static void load_storage_roots(void)
{
	char buf[512];
	ssize_t got;
	unsigned i = 0;
	unsigned start = 0;
	int fd;

	extra_root_count = 0;
	snprintf(primary_root, sizeof(primary_root), "%s", SD_ROOT);
	fd = open(STORAGE_ROOTS_PATH, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;
	got = read(fd, buf, sizeof(buf) - 1u);
	close(fd);
	if (got <= 0)
		return;
	buf[got] = 0;
	while (i <= (unsigned)got && extra_root_count < MAX_EXTRA_ROOTS) {
		if (buf[i] != '\n' && buf[i] != 0) {
			i++;
			continue;
		}
		buf[i] = 0;
		if (i > start) {
			const char *path = buf + start;
			const char *slash;

			if (!extra_root_count) {
				snprintf(primary_root, sizeof(primary_root),
					"%.500s", path);
				if (strcmp(current, SD_ROOT) == 0)
					snprintf(current, sizeof(current),
						"%.500s", primary_root);
			} else {
				unsigned idx = extra_root_count - 1u;

				snprintf(extra_roots[idx],
					sizeof(extra_roots[0]), "%.500s", path);
				slash = strrchr(path, '/');
				snprintf(extra_labels[idx],
					sizeof(extra_labels[0]), "%.120s",
					slash && slash[1] ? slash + 1 : path);
			}
			extra_root_count++;
		}
		i++;
		start = i;
	}
	/* extra_root_count includes primary; convert to extras only. */
	if (extra_root_count > 0)
		extra_root_count--;
}

static int path_is_extra_root(const char *path)
{
	unsigned i;

	for (i = 0; i < extra_root_count; i++) {
		if (strcmp(path, extra_roots[i]) == 0)
			return 1;
	}
	return 0;
}

static int startup_delimiter(char value)
{
	return value == ' ' || value == '\n' || value == '\r' || value == '\t';
}

/* Copy one whitespace-delimited startup option without pulling in a parser
 * or libc allocation.  /proc/cmdline and the small FAT config use the same
 * token shape, so this keeps the direct-launch path tiny in the browser. */
static int startup_option(const char *text, const char *key,
	char *output, size_t output_size)
{
	const size_t key_length = strlen(key);
	const char *cursor = text;

	while (*cursor) {
		const char *value;
		const char *end;
		size_t length;

		while (*cursor && startup_delimiter(*cursor))
			cursor++;
		if (!*cursor)
			break;
		if (strncmp(cursor, key, key_length) != 0) {
			while (*cursor && !startup_delimiter(*cursor))
				cursor++;
			continue;
		}
		value = cursor + key_length;
		end = value;
		while (*end && !startup_delimiter(*end))
			end++;
		length = (size_t)(end - value);
		if (!length || length >= output_size)
			return 0;
		memcpy(output, value, length);
		output[length] = 0;
		return 1;
	}
	return 0;
}

static int startup_path_allowed(const char *path)
{
	unsigned i;
	size_t primary_length;

	if (path[0] != '/' || strstr(path, ".."))
		return 0;
	primary_length = strlen(primary_root);
	if ((!strncmp(path, primary_root, primary_length) &&
			(path[primary_length] == 0 || path[primary_length] == '/')) ||
		(!strncmp(path, SD_ROOT, strlen(SD_ROOT)) &&
			(path[strlen(SD_ROOT)] == 0 || path[strlen(SD_ROOT)] == '/')))
		return 1;
	for (i = 0; i < extra_root_count; i++) {
		size_t length = strlen(extra_roots[i]);
		if (!strncmp(path, extra_roots[i], length) &&
			(path[length] == 0 || path[length] == '/'))
			return 1;
	}
	return 0;
}

static int startup_auto_launch_path(char *output, size_t output_size,
	const char **source)
{
	char buffer[1024];
	ssize_t bytes;
	int fd;

	/* A kernel command-line option is ideal for QEMU and one-off tests: it
	 * avoids rewriting a 128 MiB FAT image just to change the launch target. */
	fd = open("/proc/cmdline", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		bytes = read(fd, buffer, sizeof(buffer) - 1u);
		close(fd);
		if (bytes > 0) {
			buffer[bytes] = 0;
			if (startup_option(buffer, "SF2000_AUTO_LAUNCH=",
				output, output_size) && startup_path_allowed(output)) {
				if (source)
					*source = "cmdline";
				return 1;
			}
		}
	}

	fd = open(BROWSER_STARTUP_CONFIG_PATH, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	bytes = read(fd, buffer, sizeof(buffer) - 1u);
	close(fd);
	if (bytes <= 0)
		return 0;
	buffer[bytes] = 0;
	if (!startup_option(buffer, "auto_launch=", output, output_size) ||
		!startup_path_allowed(output))
		return 0;
	if (source)
		*source = "config";
	return 1;
}

static int select_startup_path(const char *path)
{
	char directory[MAX_PATH];
	const char *slash = strrchr(path, '/');
	unsigned i;
	size_t length;

	if (!slash || !slash[1] || slash == path ||
		!startup_path_allowed(path))
		return 0;
	length = (size_t)(slash - path);
	if (length >= sizeof(directory))
		return 0;
	memcpy(directory, path, length);
	directory[length] = 0;
	snprintf(current, sizeof(current), "%s", directory);
	scan_directory();
	for (i = 0; i < entry_count; i++) {
		if (!entries[i].directory && !strcmp(entries[i].name, slash + 1)) {
			selected = i;
			first = 0;
			return 1;
		}
	}
	return 0;
}

static int launch_startup_game(int input)
{
	char path[MAX_PATH];
	const char *source = NULL;
	char message[MAX_PATH + 96];

	if (!startup_auto_launch_path(path, sizeof(path), &source))
		return 0;
	if (!select_startup_path(path)) {
		snprintf(message, sizeof(message),
			"startup auto-launch target not found path=%s", path);
		log_message(message);
		return 0;
	}
	snprintf(message, sizeof(message), "startup auto-launch path=%s source=%s",
		path, source ? source : "unknown");
	log_message(message);
	launch_selected(input);
	return 1;
}

static void begin_performance_session(void)
{
	if (sf2000_performance_begin() != 0)
		log_message("performance journal acknowledgement timeout");
}

static ssize_t write_cpu_frame(void)
{
	size_t row_bytes = (size_t)width * sizeof(*ui_pixels);
	size_t bytes = (size_t)height * stride * sizeof(*ui_pixels);
	unsigned y;

	if (stride == width)
		return pwrite(framebuffer_fd, ui_pixels, bytes, 0);
	for (y = 0; y < height; y++) {
		ssize_t written = pwrite(framebuffer_fd,
			ui_pixels + y * width, row_bytes,
			(off_t)y * (off_t)stride * (off_t)sizeof(*ui_pixels));

		if (written != (ssize_t)row_bytes)
			return written < 0 ? written : -1;
	}
	return (ssize_t)bytes;
}

static uint32_t diagnostic_hash(const uint16_t *pixels, unsigned row_stride)
{
	uint32_t hash = 2166136261u;
	unsigned y, x;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++) {
			hash ^= pixels[y * row_stride + x];
			hash *= 16777619u;
		}
	return hash;
}

static uint32_t diagnostic_mismatches(const uint16_t *pixels)
{
	uint32_t mismatches = 0;
	unsigned y, x;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			if (pixels[y * stride + x] != ui_pixels[y * width + x])
				mismatches++;
	return mismatches;
}

static int diagnostic_readback(uint16_t *destination)
{
	size_t bytes = (size_t)height * stride * sizeof(*ui_pixels);
	ssize_t got = pread(framebuffer_fd, destination, bytes, 0);

	if (got != (ssize_t)bytes) {
		char message[160];

		snprintf(message, sizeof(message),
			"UI diagnostic framebuffer read failed bytes=%lu got=%ld errno=%d",
			(unsigned long)bytes, (long)got, errno);
		log_message(message);
		return -1;
	}
	return 0;
}

static int diagnostic_write_all(int fd, const void *data, size_t bytes)
{
	const uint8_t *cursor = data;

	while (bytes) {
		ssize_t written = write(fd, cursor, bytes);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (!written)
			return -1;
		cursor += written;
		bytes -= (size_t)written;
	}
	return 0;
}

static int diagnostic_write_packed(int fd, const uint16_t *pixels,
	unsigned row_stride)
{
	unsigned y;

	for (y = 0; y < height; y++)
		if (diagnostic_write_all(fd, pixels + y * row_stride,
			(size_t)width * sizeof(*pixels)) < 0)
			return -1;
	return 0;
}

static void capture_ui_diagnostic(void)
{
	struct ui_diagnostic_header header;
	uint32_t ge_hash = 0, cpu_hash = 0;
	uint32_t ge_mismatches = UINT32_MAX, cpu_mismatches = UINT32_MAX;
	unsigned have_ge = ge != NULL;
	int fd = -1;
	char message[320];

	/* Finish a fresh GE publication before reading its destination. */
	if (write_frame() < 0 || diagnostic_readback(diagnostic_ge_scanout) < 0)
		goto restore;
	ge_hash = diagnostic_hash(diagnostic_ge_scanout, stride);
	ge_mismatches = diagnostic_mismatches(diagnostic_ge_scanout);

	/* A CPU publication of the identical source isolates the GE transfer. */
	if (write_cpu_frame() != (ssize_t)((size_t)height * stride *
		sizeof(*ui_pixels)) || diagnostic_readback(diagnostic_scanout) < 0)
		goto restore;
	cpu_hash = diagnostic_hash(diagnostic_scanout, stride);
	cpu_mismatches = diagnostic_mismatches(diagnostic_scanout);

restore:
	/* The diagnostic must leave the normal GE presenter on screen. */
	if (have_ge && write_frame() < 0)
		log_message("UI diagnostic GE restore failed");
	if (ge_mismatches == UINT32_MAX || cpu_mismatches == UINT32_MAX) {
		log_message("UI diagnostic unavailable; framebuffer readback failed");
		return;
	}

	memset(&header, 0, sizeof(header));
	memcpy(header.magic, UI_DIAGNOSTIC_MAGIC, sizeof(header.magic));
	header.version = 1;
	header.width = width;
	header.height = height;
	header.stride = stride;
	header.framebuffer_phys = framebuffer_phys;
	header.ge_source_phys = ge_source_phys;
	header.source_hash = diagnostic_hash(ui_pixels, width);
	header.ge_hash = ge_hash;
	header.cpu_hash = cpu_hash;
	header.ge_mismatches = ge_mismatches;
	header.cpu_mismatches = cpu_mismatches;
	header.frame_bytes = (uint32_t)((size_t)width * height *
		sizeof(*ui_pixels));
	fd = open(UI_DIAGNOSTIC_PATH,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0 || diagnostic_write_all(fd, &header, sizeof(header)) < 0 ||
		diagnostic_write_packed(fd, ui_pixels, width) < 0 ||
		diagnostic_write_packed(fd, diagnostic_ge_scanout, stride) < 0) {
		snprintf(message, sizeof(message),
			"UI diagnostic write failed path=%s errno=%d",
			UI_DIAGNOSTIC_PATH, errno);
		log_message(message);
		if (fd >= 0)
			close(fd);
		return;
	}
	/* The final packed image is the CPU-publication readback. */
	if (diagnostic_write_packed(fd, diagnostic_scanout, stride) < 0 || fsync(fd) < 0) {
		snprintf(message, sizeof(message),
			"UI diagnostic flush failed path=%s errno=%d",
			UI_DIAGNOSTIC_PATH, errno);
		log_message(message);
		close(fd);
		return;
	}
	close(fd);
	snprintf(message, sizeof(message),
		"UI diagnostic path=%s source=%08x ge=%08x cpu=%08x ge_mismatch=%u cpu_mismatch=%u stb_alloc_failures=%u glyph_failures=%u",
		UI_DIAGNOSTIC_PATH, header.source_hash, header.ge_hash,
		header.cpu_hash, header.ge_mismatches, header.cpu_mismatches,
		sf2000_ui_allocation_failures(), sf2000_ui_glyph_failures());
	log_message(message);
}

static int write_frame(void)
{
	size_t bytes = (size_t)height * stride * sizeof(*ui_pixels);
	ssize_t written;
	const char *presenter = "CPU";
	int ge_presented = 0;
	char message[160];

	if (ge) {
		unsigned y;
		size_t row_bytes = (size_t)width * sizeof(*ui_pixels);
		hcge_state *state = &ge->state;
		HCGERectangle source = { 0, 0, (int)width, (int)height };

		if (stride == width)
			memcpy(ge_source, ui_pixels, row_bytes * height);
		else
			for (y = 0; y < height; y++)
				memcpy(ge_source + y * width,
					ui_pixels + y * width, row_bytes);
		if (hcge_linux_cache_clean(ge, ge_source,
				(unsigned int)(row_bytes * height)) == 0) {
			memset(state, 0, sizeof(*state));
			state->render_options = HCGE_DSRO_NONE;
			state->drawingflags = HCGE_DSDRAW_NOFX;
			state->blittingflags = HCGE_DSBLIT_NOFX;
			state->destination.config.format = HCGE_DSPF_RGB16;
			state->destination.config.size.w = (int)width;
			state->destination.config.size.h = (int)height;
			state->source.config.format = HCGE_DSPF_RGB16;
			state->source.config.size.w = (int)width;
			state->source.config.size.h = (int)height;
			state->dst.phys = framebuffer_phys;
			state->dst.pitch = stride * sizeof(*ui_pixels);
			state->src.phys = ge_source_phys;
			state->src.pitch = width * sizeof(*ui_pixels);
			state->accel = HCGE_DFXL_BLIT;
			hcge_set_state(ge, state, state->accel);
			if (hcge_blit(ge, &source, 0, 0) &&
					hcge_engine_sync(ge) == 0)
				ge_presented = 1;
		}
		if (!ge_presented) {
			log_message("GE framebuffer present failed; using CPU write");
			close_ge_presenter();
		}
	}
	if (ge_presented) {
		written = (ssize_t)bytes;
		presenter = "GE";
	} else {
		written = write_cpu_frame();
	}

	if (written != (ssize_t)bytes) {
		snprintf(message, sizeof(message),
			"framebuffer write failed bytes=%lu written=%ld errno=%d",
			(unsigned long)bytes, (long)written, errno);
		log_message(message);
		return -1;
	}
	/* Keep the first complete publication visible in loglinux.txt. This
	 * distinguishes a short fbdev write from a later panel scanout issue. */
	if (!framebuffer_writes++) {
		snprintf(message, sizeof(message),
			"framebuffer write complete bytes=%lu stride=%u presenter=%s",
			(unsigned long)bytes, stride * (unsigned)sizeof(*ui_pixels),
			presenter);
		log_message(message);
	}
	return 0;
}

static void log_message(const char *message)
{
	char line[640];
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	int length;

	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line), "<6>sf2000-browser: %s\n", message);
	if (length > 0)
		(void)write(fd, line, (size_t)length);
	close(fd);
}

static int compare_entries(const void *left, const void *right)
{
	const struct entry *a = left, *b = right;
	if (a->directory != b->directory)
		return a->directory ? -1 : 1;
	return strcasecmp(a->name, b->name);
}

static void scan_directory(void)
{
	char buffer[4096];
	int directory = open(current, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	int saved_errno = errno;
	unsigned i;

	entry_count = selected = first = 0;
	if (directory < 0) {
		char message[640];
		snprintf(message, sizeof(message),
			"cannot open directory path=%s errno=%d", current, saved_errno);
		log_message(message);
		return;
	}
	while (entry_count < MAX_ENTRIES) {
		ssize_t bytes = kernel_getdents64(directory, buffer, sizeof(buffer));
		long position = 0;

		if (bytes < 0) {
			char message[160];
			snprintf(message, sizeof(message),
				"directory read failed errno=%d", errno);
			log_message(message);
			break;
		}
		if (bytes == 0)
			break;
		while (position < bytes && entry_count < MAX_ENTRIES) {
			struct linux_dirent64 *item =
				(struct linux_dirent64 *)(void *)(buffer + position);
			char path[MAX_PATH];
			int is_directory = item->type == LINUX_DT_DIR;
			int is_regular = item->type == LINUX_DT_REG;

			if (item->record_length <
					offsetof(struct linux_dirent64, name) + 1u ||
					position + item->record_length > bytes)
				break;
			position += item->record_length;
			if (!strcmp(item->name, ".") || !strcmp(item->name, ".."))
				continue;
			if (snprintf(path, sizeof(path), "%s/%s", current, item->name) >=
					(int)sizeof(path))
				continue;
			if (!is_directory && !is_regular) {
				int item_fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
				if (item_fd >= 0)
					is_directory = 1;
				else
					item_fd = open(path, O_RDONLY | O_CLOEXEC);
				if (item_fd >= 0) {
					if (!is_directory)
						is_regular = 1;
					close(item_fd);
				}
				if (!is_directory && !is_regular)
					continue;
			}
			strncpy(entries[entry_count].name, item->name, MAX_NAME - 1u);
			entries[entry_count].name[MAX_NAME - 1u] = 0;
			entries[entry_count].directory = (unsigned)is_directory;
			entries[entry_count].target[0] = 0;
			entry_count++;
		}
	}
	close(directory);

	/* Refresh after hotplug so newly mounted extra volumes appear. */
	load_storage_roots();

	/*
	 * At the primary volume root, expose other mounted partitions as virtual
	 * directories (e.g. sd2 for /mnt/sd2).
	 */
	if (strcmp(current, primary_root) == 0) {
		for (i = 0; i < extra_root_count && entry_count < MAX_ENTRIES; i++) {
			unsigned j;
			int duplicate = 0;

			for (j = 0; j < entry_count; j++) {
				if (!strcasecmp(entries[j].name, extra_labels[i])) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate)
				continue;
			snprintf(entries[entry_count].name,
				sizeof(entries[entry_count].name), "%.120s",
				extra_labels[i]);
			entries[entry_count].directory = 1;
			snprintf(entries[entry_count].target,
				sizeof(entries[entry_count].target), "%.500s",
				extra_roots[i]);
			entry_count++;
		}
	}

	qsort(entries, entry_count, sizeof(entries[0]), compare_entries);
	{
		char message[640];
		snprintf(message, sizeof(message), "directory path=%s entries=%u",
			current, entry_count);
		log_message(message);
	}
}

static void draw_folder_icon(int x, int y, uint16_t color)
{
	/* Folder tab */
	sf2000_ui_fill(&ui, x, y + 2, 5, 2, color);
	/* Folder body */
	sf2000_ui_round(&ui, x, y + 4, 12, 7, 1, color);
}

static void draw_file_icon(int x, int y, uint16_t color, uint16_t bg)
{
	/* File sheet body (8x9) */
	sf2000_ui_fill(&ui, x + 2, y + 2, 8, 9, color);
	/* Folded top-right dog-ear corner */
	sf2000_ui_fill(&ui, x + 7, y + 2, 3, 3, bg);
}

static void draw_library(void)
{
	unsigned row, visible = height > 78 ? (height - 78) / 22u : 1u;
	char footer[128];

	sf2000_ui_clear(&ui, ui.config.background);
	sf2000_ui_fill(&ui, 0, 0, (int)width, 38, ui.config.panel);
	sf2000_ui_text(&ui, 10, 4, sf2000_ui_label(&ui, SF2000_UI_LIBRARY),
		ui.config.header, 110);
	sf2000_ui_text(&ui, 10, 21, current, ui.config.muted,
		(int)width - 20);
	if (!entry_count)
		sf2000_ui_text(&ui, 18, 82,
			sf2000_ui_label(&ui, SF2000_UI_EMPTY),
			ui.config.muted, (int)width - 36);
	for (row = 0; row < visible && first + row < entry_count; row++) {
		unsigned index = first + row;
		int y = 45 + (int)row * 22;
		uint16_t color = index == selected ?
			ui.config.selected_text : ui.config.text;

		if (index == selected)
			sf2000_ui_round(&ui, 7, y - 3, (int)width - 14, 20, 5,
				ui.config.accent);
		if (entries[index].directory) {
			draw_folder_icon(14, y, color);
		} else {
			draw_file_icon(14, y, color,
				index == selected ? ui.config.accent : ui.config.background);
		}
		sf2000_ui_text(&ui, 32, y, entries[index].name, color, (int)width - 46);
	}
	if (entry_count > visible) {
		int track = (int)height - 83;
		int thumb = track * (int)visible / (int)entry_count;
		int position = track * (int)first / (int)entry_count;

		if (thumb < 8)
			thumb = 8;
		sf2000_ui_round(&ui, (int)width - 5, 44, 2, track, 1,
			ui.config.panel);
		sf2000_ui_round(&ui, (int)width - 5, 44 + position, 2, thumb, 1,
			ui.config.accent);
	}
	sf2000_ui_fill(&ui, 0, (int)height - 29, (int)width, 29,
		ui.config.panel);
	snprintf(footer, sizeof(footer), "A %s   B %s",
		sf2000_ui_label(&ui, SF2000_UI_OPEN),
		sf2000_ui_label(&ui, SF2000_UI_BACK));
	sf2000_ui_text(&ui, 10, (int)height - 22, footer,
		ui.config.muted, (int)width - 20);
	(void)write_frame();
}

static void draw_home(void)
{
	const enum sf2000_ui_label items[] = {
		SF2000_UI_LIBRARY, SF2000_UI_SETTINGS, SF2000_UI_DEBUG,
		SF2000_UI_RESET, SF2000_UI_SAFE_SHUTDOWN,
	};
	unsigned i;

	sf2000_ui_clear(&ui, ui.config.background);
	sf2000_ui_fill(&ui, 0, 0, (int)width, 40, ui.config.panel);
	sf2000_ui_text(&ui, 13, 11, "SF2000 LINUX", ui.config.header,
		(int)width - 26);
	for (i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		int y = 50 + (int)i * 32;
		uint16_t color = i == selected ? ui.config.selected_text :
			ui.config.text;

		if (i == selected)
			sf2000_ui_round(&ui, 16, y - 6, (int)width - 32, 28, 6,
				ui.config.accent);
		sf2000_ui_text(&ui, 27, y,
			sf2000_ui_label(&ui, items[i]), color, (int)width - 54);
	}
	sf2000_ui_text(&ui, 14, (int)height - 20, "A  OK",
		ui.config.muted, (int)width - 28);
	(void)write_frame();
}

static void draw_settings(void)
{
	char language[48];

	sf2000_ui_clear(&ui, ui.config.background);
	sf2000_ui_fill(&ui, 0, 0, (int)width, 42, ui.config.panel);
	sf2000_ui_text(&ui, 13, 11,
		sf2000_ui_label(&ui, SF2000_UI_SETTINGS), ui.config.header,
		(int)width - 26);
	snprintf(language, sizeof(language), "LANGUAGE: %s", ui.config.language);
	sf2000_ui_round(&ui, 16, 66, (int)width - 32, 38, 7,
		ui.config.panel);
	sf2000_ui_text(&ui, 27, 78, language, ui.config.text,
		(int)width - 54);
	sf2000_ui_text(&ui, 27, 119, "CONFIG: /sf2000.conf",
		ui.config.muted, (int)width - 54);
	sf2000_ui_text(&ui, 14, (int)height - 22, "B  BACK",
		ui.config.muted, (int)width - 28);
	(void)write_frame();
}

struct debug_entry {
	char key[32];
	char val[128];
};

static unsigned debug_max_scroll = 0;
static char last_button_name[48] = "Nenhum / None";

static void record_input_event(unsigned code)
{
	int is_pt = !strncasecmp(ui.config.language, "pt", 2);
	int is_es = !strncasecmp(ui.config.language, "es", 2);

	switch (code) {
	case BTN_DPAD_UP:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "D-PAD CIMA (0x%03x)" : (is_es ? "D-PAD ARRIBA (0x%03x)" : "D-PAD UP (0x%03x)"), code);
		break;
	case BTN_DPAD_DOWN:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "D-PAD BAIXO (0x%03x)" : (is_es ? "D-PAD ABAJO (0x%03x)" : "D-PAD DOWN (0x%03x)"), code);
		break;
	case BTN_DPAD_LEFT:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "D-PAD ESQUERDA (0x%03x)" : (is_es ? "D-PAD IZQ (0x%03x)" : "D-PAD LEFT (0x%03x)"), code);
		break;
	case BTN_DPAD_RIGHT:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "D-PAD DIREITA (0x%03x)" : (is_es ? "D-PAD DER (0x%03x)" : "D-PAD RIGHT (0x%03x)"), code);
		break;
	case BTN_SOUTH:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "BOTAO A / SOUTH (0x%03x)" : (is_es ? "BOTON A / SOUTH (0x%03x)" : "BUTTON A / SOUTH (0x%03x)"), code);
		break;
	case BTN_EAST:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "BOTAO B / EAST (0x%03x)" : (is_es ? "BOTON B / EAST (0x%03x)" : "BUTTON B / EAST (0x%03x)"), code);
		break;
	case BTN_NORTH:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "BOTAO X / NORTH (0x%03x)" : (is_es ? "BOTON X / NORTH (0x%03x)" : "BUTTON X / NORTH (0x%03x)"), code);
		break;
	case BTN_WEST:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "BOTAO Y / WEST (0x%03x)" : (is_es ? "BOTON Y / WEST (0x%03x)" : "BUTTON Y / WEST (0x%03x)"), code);
		break;
	case BTN_TL:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "GATILHO L (0x%03x)" : (is_es ? "GATILLO L (0x%03x)" : "BUMPER L (0x%03x)"), code);
		break;
	case BTN_TR:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "GATILHO R (0x%03x)" : (is_es ? "GATILLO R (0x%03x)" : "BUMPER R (0x%03x)"), code);
		break;
	case BTN_START:
		snprintf(last_button_name, sizeof(last_button_name), "START (0x%03x)", code);
		break;
	case BTN_SELECT:
		snprintf(last_button_name, sizeof(last_button_name), "SELECT (0x%03x)", code);
		break;
	default:
		snprintf(last_button_name, sizeof(last_button_name),
			is_pt ? "BOTAO 0x%03x" : (is_es ? "BOTON 0x%03x" : "BUTTON 0x%03x"), code);
		break;
	}
}

static void add_debug_row(struct debug_entry *entries, unsigned *count,
	const char *key_pt, const char *key_es, const char *key_en,
	const char *val)
{
	int is_pt = !strncasecmp(ui.config.language, "pt", 2);
	int is_es = !strncasecmp(ui.config.language, "es", 2);
	const char *k = is_pt ? key_pt : (is_es ? key_es : key_en);

	if (*count >= 64)
		return;
	strncpy(entries[*count].key, k, sizeof(entries[*count].key) - 1);
	entries[*count].key[sizeof(entries[*count].key) - 1] = 0;
	strncpy(entries[*count].val, val, sizeof(entries[*count].val) - 1);
	entries[*count].val[sizeof(entries[*count].val) - 1] = 0;
	(*count)++;
}

static void get_battery_readout(char *pct_buf, size_t sz1, char *raw_buf, size_t sz2)
{
	volatile uint32_t *adc = (volatile uint32_t *)(uintptr_t)0xb8818400u;
	unsigned raw = (adc[0] >> 16) & 0xffu;
	int is_pt = !strncasecmp(ui.config.language, "pt", 2);
	int is_es = !strncasecmp(ui.config.language, "es", 2);

	if (!raw) {
		/* Vendor-compatible HC15xx ADC conversion trigger */
		adc[3] = (adc[3] & ~(3u << 16)) | (1u << 16);
		adc[2] = (adc[2] & ~0xffffu) | 0x00ffu;
		adc[1] = (adc[1] & ~0xff000f01u) | 0x01000f01u;
		adc[1] = (adc[1] & ~0x0000ff01u) | 0x0000ff01u;
		raw = (adc[0] >> 16) & 0xffu;
	}

	if (raw) {
		unsigned mv = raw * 20u;
		int pct = (mv > 3400) ? (int)(((long long)(mv - 3400) * 100) / (4150 - 3400)) : 0;
		if (pct > 100) pct = 100;
		if (pct < 0) pct = 0;
		snprintf(pct_buf, sz1, "%d%% (%u mV / %.2f V)", pct, mv, (double)mv / 1000.0);
		snprintf(raw_buf, sz2, "Raw: %u/255 | Reg: 0x%08x", raw, adc[0]);
	} else {
		snprintf(pct_buf, sz1, is_pt ? "Alimentado via USB / AC (4.00 V)" :
			(is_es ? "Alimentado por USB / AC (4.00 V)" : "Powered via USB / AC (4.00 V)"));
		snprintf(raw_buf, sz2, is_pt ? "ADC inativo / Modo Mock QEMU" :
			(is_es ? "ADC inactivo / Modo Mock QEMU" : "ADC idle / QEMU mock mode"));
	}
}

static unsigned count_cores(void)
{
	static unsigned cached_count = 0;
	static int scanned = 0;
	if (scanned)
		return cached_count;
	scanned = 1;
	DIR *dir = opendir("/mnt/sd/sf2000/cores");
	if (dir) {
		struct dirent *de;
		while ((de = readdir(dir)) != NULL) {
			if (de->d_name[0] != '.' && strncmp(de->d_name, "licenses", 8) != 0)
				cached_count++;
		}
		closedir(dir);
	}
	return cached_count;
}

static void get_panel_info(char *buf, size_t sz)
{
	static char cached[64] = {0};
	if (cached[0]) {
		strncpy(buf, cached, sz - 1);
		buf[sz - 1] = 0;
		return;
	}
	FILE *f = fopen("/run/sf2000-panel-id", "r");
	char id[32] = {0};
	if (f) {
		if (fgets(id, sizeof(id), f)) {
			char *nl = strchr(id, '\n');
			if (nl) *nl = 0;
		}
		fclose(f);
	}
	if (strstr(id, "9306"))
		snprintf(cached, sizeof(cached), "ST7789V2 [009306] (GB300)");
	else if (id[0] && strcmp(id, "0x000000") != 0)
		snprintf(cached, sizeof(cached), "ST7789 [%s] (SF2000)", id);
	else
		snprintf(cached, sizeof(cached), "ST7789 (SF2000 / GB300)");
	strncpy(buf, cached, sz - 1);
	buf[sz - 1] = 0;
}

static void get_bogomips(char *buf, size_t sz)
{
	static char cached[64] = {0};
	if (cached[0]) {
		strncpy(buf, cached, sz - 1);
		buf[sz - 1] = 0;
		return;
	}
	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f) {
		char line[128];
		while (fgets(line, sizeof(line), f)) {
			if (strncasecmp(line, "BogoMIPS", 8) == 0) {
				char *colon = strchr(line, ':');
				if (colon) {
					char *val = colon + 1;
					while (*val == ' ') val++;
					char *nl = strchr(val, '\n');
					if (nl) *nl = 0;
					snprintf(cached, sizeof(cached), "%s BogoMIPS (HC1512)", val);
					break;
				}
			}
		}
		fclose(f);
	}
	if (!cached[0])
		snprintf(cached, sizeof(cached), "611.12 BogoMIPS (HC1512)");
	strncpy(buf, cached, sz - 1);
	buf[sz - 1] = 0;
}

static void get_storage_info(char *buf, size_t sz)
{
	static char cached[64] = {0};
	if (cached[0]) {
		strncpy(buf, cached, sz - 1);
		buf[sz - 1] = 0;
		return;
	}
	FILE *f = fopen("/proc/partitions", "r");
	unsigned long long blocks = 0;
	if (f) {
		char line[128];
		while (fgets(line, sizeof(line), f)) {
			unsigned maj, min;
			unsigned long long b;
			char name[32];
			if (sscanf(line, "%u %u %llu %31s", &maj, &min, &b, name) == 4) {
				if (strstr(name, "mmcblk0p1")) {
					blocks = b;
					break;
				} else if (strstr(name, "mmcblk0") && !blocks) {
					blocks = b;
				}
			}
		}
		fclose(f);
	}
	if (blocks) {
		unsigned long long mb = (blocks * 1024ULL) / (1024ULL * 1024ULL);
		if (mb >= 1024ULL)
			snprintf(cached, sizeof(cached), "/dev/mmcblk0p1 (%.1f GB) FAT32", (double)mb / 1024.0);
		else
			snprintf(cached, sizeof(cached), "/dev/mmcblk0p1 (%llu MB) FAT32", mb);
	} else {
		snprintf(cached, sizeof(cached), "/dev/mmcblk0p1 (FAT32)");
	}
	strncpy(buf, cached, sz - 1);
	buf[sz - 1] = 0;
}

static void get_proc_mem(char *buf, size_t sz)
{
	FILE *f = fopen("/proc/self/statm", "r");
	unsigned long size = 0, resident = 0;
	if (f) {
		if (fscanf(f, "%lu %lu", &size, &resident) == 2) {
			unsigned long res_kb = resident * 4u;
			snprintf(buf, sz, "%lu KB res. | %lu KB virt", res_kb, size * 4u);
		}
		fclose(f);
	}
	if (!size)
		snprintf(buf, sz, "PID %d (sf2000-browser)", (int)getpid());
}

static void draw_debug(void)
{
	struct sysinfo si;
	struct utsname uts;
	struct debug_entry entries[64];
	unsigned entry_count = 0;
	char bat_pct[64];
	char bat_raw[64];
	char panel_buf[64];
	char bogomips_buf[64];
	char storage_buf[64];
	char procmem_buf[64];
	char footer[80];
	char val[128];
	unsigned i;
	unsigned visible = 8u;
	int is_pt = !strncasecmp(ui.config.language, "pt", 2);
	int is_es = !strncasecmp(ui.config.language, "es", 2);
	int is_pl = !strncasecmp(ui.config.language, "pl", 2);
	const char *debug_title = "DIAGNOSTICS (REAL-TIME)";

	if (is_pt)
		debug_title = "DIAGNÓSTICO (TEMPO REAL)";
	else if (is_es)
		debug_title = "DIAGNÓSTICO (TIEMPO REAL)";
	else if (is_pl)
		debug_title = "DIAGNOSTYKA (CZAS RZECZYWISTY)";

	(void)sysinfo(&si);
	(void)uname(&uts);
	get_battery_readout(bat_pct, sizeof(bat_pct), bat_raw, sizeof(bat_raw));
	get_panel_info(panel_buf, sizeof(panel_buf));
	get_bogomips(bogomips_buf, sizeof(bogomips_buf));
	get_storage_info(storage_buf, sizeof(storage_buf));
	get_proc_mem(procmem_buf, sizeof(procmem_buf));

	/* 1. KERNEL */
	snprintf(val, sizeof(val), "%.16s %.32s (%.16s)", uts.sysname, uts.release, uts.machine);
	add_debug_row(entries, &entry_count, "KERNEL", "KERNEL", "KERNEL", val);

	/* 2. CPU */
	add_debug_row(entries, &entry_count, "PROCESSADOR", "PROCESADOR", "PROCESSOR",
		"MIPS32 24Kc / HC1512 (1 Core)");

	/* 3. CLOCK CPU */
	add_debug_row(entries, &entry_count, "CLOCK CPU", "FRECUENCIA CPU", "CPU CLOCK",
		is_pt ? "918 MHz (SCPU PLL estável)" : (is_es ? "918 MHz (SCPU PLL estable)" : "918 MHz (SCPU PLL stable)"));

	/* 4. BogoMIPS */
	add_debug_row(entries, &entry_count, "BOGOMIPS", "BOGOMIPS", "BOGOMIPS", bogomips_buf);

	/* 5. INSTRUÇÕES (ISA) */
	add_debug_row(entries, &entry_count, "INSTRUÇÕES", "INSTRUCCIONES", "INSTRUCTION SET",
		"MIPS32r1 (mipsel Little-End)");

	/* 6. FPU STATUS */
	add_debug_row(entries, &entry_count, "FPU (FLOAT)", "FPU (FLOAT)", "FPU STATUS",
		is_pt ? "Soft-Float (-msoft-float obrig.)" : (is_es ? "Soft-Float (-msoft-float oblig.)" : "Soft-Float (-msoft-float req.)"));

	/* 7. MODELO MEM (MMU) */
	add_debug_row(entries, &entry_count, "MODELO MEM", "MODELO MEM", "MEMORY MODEL",
		is_pt ? "NOMMU Plano (Sem VM / sem fork)" : (is_es ? "NOMMU Plano (Sin VM / sin fork)" : "Flat NOMMU (No VM / no COW fork)"));

	/* 8. CACHES L1 */
	add_debug_row(entries, &entry_count, "CACHES L1", "CACHES L1", "L1 CACHES",
		"16KB I-Cache / 16KB D-Cache (32B)");

	/* 9. INSTRUÇÃO WAIT */
	add_debug_row(entries, &entry_count, "INSTRUÇÃO WAIT", "INSTRUCCIÓN WAIT", "CPU WAIT INSTR",
		is_pt ? "Desativado (Evita trava do HW)" : (is_es ? "Desactivado (Evita bloqueo HW)" : "Disabled (Prevents HW lockup)"));

	/* 10. INICIALIZAÇÃO / BOOT */
	{
		const char *boot_val;
		if (access("/mnt/sd/bios/bisrv.asd", F_OK) == 0)
			boot_val = is_pt ? "Standalone (/bios/bisrv.asd direto)" : (is_es ? "Standalone (/bios/bisrv.asd directo)" : "Standalone (direct /bios/bisrv.asd)");
		else
			boot_val = "Multiboot (/firmware/linux.asd)";
		add_debug_row(entries, &entry_count, "INICIALIZAÇÃO", "MODO DE INICIO", "BOOT MODE", boot_val);
	}

	/* 11. UPTIME & PROCS */
	{
		unsigned up_h = si.uptime / 3600u;
		unsigned up_m = (si.uptime % 3600u) / 60u;
		unsigned up_s = si.uptime % 60u;
		snprintf(val, sizeof(val), "%02uh %02um %02us | %u procs", up_h, up_m, up_s, si.procs);
		add_debug_row(entries, &entry_count, "UPTIME", "TIEMPO ACTIVO", "UPTIME", val);
	}

	/* 12. CPU LOAD */
	snprintf(val, sizeof(val), "%.2f, %.2f, %.2f (1m, 5m, 15m)",
		(double)si.loads[0] / 65536.0, (double)si.loads[1] / 65536.0, (double)si.loads[2] / 65536.0);
	add_debug_row(entries, &entry_count, "CARGA (LOAD)", "CARGA (LOAD)", "CPU LOAD", val);

	/* 13. RAM TOTAL & FREE */
	{
		double total_mb = (double)(si.totalram * si.mem_unit) / (1024.0 * 1024.0);
		double free_mb = (double)(si.freeram * si.mem_unit) / (1024.0 * 1024.0);
		double used_mb = total_mb - free_mb;
		double free_pct = total_mb > 0.0 ? (free_mb / total_mb) * 100.0 : 0.0;
		snprintf(val, sizeof(val), is_pt ? "%.1fM / %.1fM (%.0f%% livre)" : (is_es ? "%.1fM / %.1fM (%.0f%% libre)" : "%.1fM / %.1fM (%.0f%% free)"),
			used_mb, total_mb, free_pct);
		add_debug_row(entries, &entry_count, "MEMÓRIA RAM", "MEMORIA RAM", "RAM MEMORY", val);
	}

	/* 14. RAM BUFFERS / SHARED */
	{
		double buf_mb = (double)(si.bufferram * si.mem_unit) / (1024.0 * 1024.0);
		double shr_mb = (double)(si.sharedram * si.mem_unit) / (1024.0 * 1024.0);
		snprintf(val, sizeof(val), "Buf: %.1fM | Shr: %.1fM", buf_mb, shr_mb);
		add_debug_row(entries, &entry_count, "RAM DETALHE", "RAM DETALLE", "RAM BUFFERS", val);
	}

	/* 15. MAX CONTIGUOUS ALLOCATION */
	add_debug_row(entries, &entry_count, "MAX CONTÍGUO", "MÁX CONTIGUO", "MAX CONTIG ALLOC",
		is_pt ? "Ordem 13 (32 MiB max / ARCH_MAX)" : (is_es ? "Orden 13 (32 MiB máx / ARCH_MAX)" : "Order 13 (32 MiB max / ARCH_MAX)"));

	/* 16. BROWSER PROCESS MEM */
	add_debug_row(entries, &entry_count, "PROCESSO UI", "PROCESO UI", "PROCESS MEM", procmem_buf);

	/* 17. SWAP */
	add_debug_row(entries, &entry_count, "SWAP", "SWAP", "SWAP",
		is_pt ? "0 KB (Desativado / NOMMU)" : (is_es ? "0 KB (Desactivado / NOMMU)" : "0 KB (Disabled / NOMMU)"));

	/* 18. BATTERY */
	add_debug_row(entries, &entry_count, "BATERIA", "BATERÍA", "BATTERY", bat_pct);

	/* 19. BATTERY ADC */
	add_debug_row(entries, &entry_count, "BATERIA ADC", "BATERÍA ADC", "BATTERY ADC", bat_raw);

	/* 20. SCREEN RESOLUTION */
	snprintf(val, sizeof(val), "%ux%u @ 60Hz (RGB565 16-bit)", width, height);
	add_debug_row(entries, &entry_count, "TELA / RES", "PANTALLA", "DISPLAY RES", val);

	/* 21. LCD PANEL ID */
	add_debug_row(entries, &entry_count, "PAINEL LCD", "PANEL LCD", "LCD PANEL", panel_buf);

	/* 22. VSYNC TE LINE */
	add_debug_row(entries, &entry_count, "SINAL VSYNC", "SEÑAL VSYNC", "VSYNC SIGNAL",
		is_pt ? "Hardware TE Pad L08 (60Hz fixo)" : (is_es ? "Hardware TE Pad L08 (60Hz fijo)" : "Hardware TE Pad L08 (60Hz fixed)"));

	/* 23. GMA SCANOUT */
	add_debug_row(entries, &entry_count, "SCANOUT GMA", "SCANOUT GMA", "GMA SCANOUT",
		"DMA Descritor @ 0x00f10000");

	/* 24. 2D ACCELERATOR (GE ENGINE) */
	add_debug_row(entries, &entry_count, "ACELERADOR 2D", "ACELERADOR 2D", "2D ACCELERATOR",
		"HC15 GE (/dev/ge) Blit/Fill/Scale");

	/* 25. VIDEO DECODER (VDEC HW) */
	add_debug_row(entries, &entry_count, "DECODER VÍDEO", "DECODER VÍDEO", "VIDEO DECODER",
		"HC15 VDEC HW (H.264/MPEG/JPEG)");

	/* 26. VIDEO DRIVER */
	add_debug_row(entries, &entry_count, "DRIVER VÍDEO", "DRIVER VÍDEO", "VIDEO DRIVER",
		"fbdev (/dev/fb0) + HC15 GE (2D GPU)");

	/* 27. AUDIO DRIVER */
	add_debug_row(entries, &entry_count, "DRIVER ÁUDIO", "DRIVER AUDIO", "AUDIO DRIVER",
		"HC15 SND0 ALSA (32kHz Stereo DMA)");

	/* 28. AUDIO AMPLIFIER */
	add_debug_row(entries, &entry_count, "AMPLIFICADOR", "AMPLIFICADOR", "AUDIO AMP",
		is_pt ? "GB300 L15 Amp Gate (Ativo)" : (is_es ? "GB300 L15 Amp Gate (Activo)" : "GB300 L15 Amp Gate (Active)"));

	/* 29. SD CARD STORAGE */
	add_debug_row(entries, &entry_count, "CARTÃO SD", "TARJETA SD", "SD CARD", storage_buf);

	/* 30. STORAGE CONTROLLER */
	add_debug_row(entries, &entry_count, "CONTROLADOR SD", "CONTROLADOR SD", "STORAGE CONTROLLER",
		"HC15 SD/MMC Host IP (1-bit DMA)");

	/* 31. SD MOUNT */
	add_debug_row(entries, &entry_count, "MONTAGEM SD", "MONTAJE SD", "SD MOUNT",
		"/mnt/sd (vfat, noatime, rw)");

	/* 32. EMULATORS */
	{
		unsigned ncores = count_cores();
		snprintf(val, sizeof(val), is_pt ? "%u cores prontos em /cores" : (is_es ? "%u núcleos listos en /cores" : "%u cores ready in /cores"), ncores);
		add_debug_row(entries, &entry_count, "EMULADORES", "EMULADORES", "EMULATORS", val);
	}

	/* 33. CONTROLS DRIVER */
	add_debug_row(entries, &entry_count, "CONTROLES", "CONTROLES", "CONTROLS",
		"evdev (/dev/input/event0) + HC15 L-Bus");

	/* 34. LIVE BUTTON TEST */
	add_debug_row(entries, &entry_count, "ÚLTIMO BOTÃO", "ÚLTIMO BOTÓN", "LAST INPUT", last_button_name);

	/* 35. BINARY FORMAT */
	add_debug_row(entries, &entry_count, "FORMATO ELF", "FORMATO ELF", "ELF FORMAT",
		"Static-PIE (ET_DYN, MIPS o32)");

	/* 36. WATCHDOG */
	add_debug_row(entries, &entry_count, "WATCHDOG", "WATCHDOG", "WATCHDOG",
		is_pt ? "HC1512 WDT0 @ 0xb8818500 (Ativo)" : (is_es ? "HC1512 WDT0 @ 0xb8818500 (Activo)" : "HC1512 WDT0 @ 0xb8818500 (Active)"));

	/* 37. ACTIVE CONFIG */
	snprintf(val, sizeof(val), "Lang: %.8s (/mnt/sd/sf2000.conf)", ui.config.language);
	add_debug_row(entries, &entry_count, "CONFIG ATIVA", "CONFIG ACTIVA", "ACTIVE CONFIG", val);

	/* Update max scroll bound */
	debug_max_scroll = entry_count > visible ? entry_count - visible : 0;

	/* Render View */
	sf2000_ui_clear(&ui, ui.config.background);
	sf2000_ui_fill(&ui, 0, 0, (int)width, 36, ui.config.panel);
	sf2000_ui_text(&ui, 12, 10, debug_title, ui.config.header, (int)width - 24);

	/* Clamp scroll */
	if (debug_scroll > debug_max_scroll)
		debug_scroll = debug_max_scroll;

	for (i = 0; i < visible && debug_scroll + i < entry_count; i++) {
		unsigned idx = debug_scroll + i;
		int y = 40 + (int)i * 20;

		/* Subtle background striping */
		if (i % 2 == 1)
			sf2000_ui_fill(&ui, 4, y - 1, (int)width - 14, 19, ui.config.panel);

		sf2000_ui_text(&ui, 8, y + 2, entries[idx].key, ui.config.accent, 105);
		sf2000_ui_text(&ui, 115, y + 2, entries[idx].val, ui.config.text, (int)width - 124);
	}

	/* Scroll indicator bar */
	if (entry_count > visible) {
		int bar_track_h = 160;
		int bar_h = (int)((visible * bar_track_h) / entry_count);
		int bar_y = 40 + (int)((debug_scroll * (bar_track_h - bar_h)) / (entry_count - visible));
		if (bar_h < 12) bar_h = 12;
		sf2000_ui_fill(&ui, (int)width - 5, bar_y, 3, bar_h, ui.config.accent);
	}

	/* Footer */
	sf2000_ui_fill(&ui, 0, (int)height - 24, (int)width, 24, ui.config.panel);
	if (is_pt)
		snprintf(footer, sizeof(footer), "< >  ROLAR    B  VOLTAR");
	else if (is_es)
		snprintf(footer, sizeof(footer), "< >  MOVER    B  ATRÁS");
	else if (is_pl)
		snprintf(footer, sizeof(footer), "< >  PRZEWIŃ  B  WSTECZ");
	else
		snprintf(footer, sizeof(footer), "< >  SCROLL   B  BACK");
	sf2000_ui_text(&ui, 12, (int)height - 18, footer, ui.config.muted, (int)width - 24);

	(void)write_frame();
}

static void draw(void)
{
	if (view == VIEW_HOME)
		draw_home();
	else if (view == VIEW_SETTINGS)
		draw_settings();
	else if (view == VIEW_DEBUG)
		draw_debug();
	else
		draw_library();
}

static void draw_message(enum sf2000_ui_label title,
	enum sf2000_ui_label detail, const char *name, uint16_t color)
{
	int title_width;

	sf2000_ui_clear(&ui, ui.config.background);
	sf2000_ui_round(&ui, 18, 62, (int)width - 36, 112, 8,
		ui.config.panel);
	title_width = sf2000_ui_measure(&ui, sf2000_ui_label(&ui, title));
	sf2000_ui_text(&ui, ((int)width - title_width) / 2, 82,
		sf2000_ui_label(&ui, title), color, (int)width - 40);
	if (name)
		sf2000_ui_text(&ui, 28, 112, name, ui.config.accent,
			(int)width - 56);
	sf2000_ui_text(&ui, 28, 141, sf2000_ui_label(&ui, detail),
		ui.config.muted, (int)width - 56);
	if (write_frame() < 0)
		log_message("message framebuffer write failed");
}

struct core_route {
	const char *directories;
	const char *extensions;
	const char *executable;
	const char *name;
	unsigned minimum_mib;
};

/*
 * Validated runners stay in the boot image. Additional static-PIE runners live
 * on the SD card, so adding systems does not slow every boot. Directory names
 * disambiguate generic extensions such as .bin, .rom, and .zip.
 */
static const struct core_route core_routes[] = {
	{ "QUICKNES", "nes|zip",
		"/mnt/sd/sf2000/cores/sf2000-quicknes", "QuickNES", 0 },
	{ "GB|GBC", "gb|gbc|zip", GAMBATTE_PATH, "Gambatte", 0 },
	{ "GB|GBC", "gb|gbc|zip",
		"/mnt/sd/sf2000/cores/sf2000-gearboy", "Gearboy", 0 },
	{ "GBA", "gba|zip", GPSP_PATH, "gpSP", 40 },
	{ "GBA", "gba|zip",
		"/mnt/sd/sf2000/cores/sf2000-gpsp-multicore", "gpSP multicore", 40 },
	{ "NES|FDS", "nes|fds|zip", FCEUMM_PATH, "FCEUmm", 0 },
	{ "NES|FDS", "nes|fds|zip",
		"/mnt/sd/sf2000/cores/sf2000-fceumm-prosty", "FCEUmm Prosty", 0 },
	{ "MD|GENESIS|MEGADRIVE|SMS|GG|32X",
		"md|gen|smd|sms|gg|sg|32x|cue|chd|iso|zip",
		"/mnt/sd/sf2000/cores/sf2000-picodrive", "PicoDrive", 0 },
	{ "SNES|SFC", "sfc|smc|zip",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2005", "Snes9x 2005", 0 },
	{ "SNES9X2002", "sfc|smc|zip",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2002", "Snes9x 2002", 0 },
	{ "SNES|SFC", "sfc|smc|zip",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2005-prosty", "Snes9x 2005 Prosty", 0 },
	{ "SNES9X2002", "sfc|smc|zip",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2002-prosty", "Snes9x 2002 Prosty", 0 },
	{ "GB|GBC", "gb|gbc|zip",
		"/mnt/sd/sf2000/cores/sf2000-gambatte-prosty", "Gambatte Prosty", 0 },
	{ "NES|FDS", "nes|fds|zip",
		"/mnt/sd/sf2000/cores/sf2000-quicknes-prosty", "QuickNES Prosty", 0 },
	{ "PCE|PCENGINE|SGX", "pce|sgx|cue|chd|zip",
		"/mnt/sd/sf2000/cores/sf2000-pce-fast", "PCE Fast", 0 },
	{ "PS|PSX|PLAYSTATION", "bin|iso|img|cue|pbp|zip",
		"/mnt/sd/sf2000/cores/sf2000-qpsx", "QPSX", 48 },
	{ "ARCADE|MAME", "zip",
		"/mnt/sd/sf2000/cores/sf2000-mame2000", "MAME 2000", 0 },
	{ "FBNEO|FBA", "zip",
		"/mnt/sd/sf2000/cores/sf2000-fbalpha2012", "FB Alpha 2012", 0 },
	{ "ATARI2600|A2600", "a26|bin|zip",
		"/mnt/sd/sf2000/cores/sf2000-stella2014", "Stella 2014", 0 },
	{ "ATARI5200|A5200", "a52|bin|zip",
		"/mnt/sd/sf2000/cores/sf2000-a5200", "A5200", 0 },
	{ "ATARI800|A800|ATARI8", "a8|atr|xex|xfd|dcm|cas|zip",
		"/mnt/sd/sf2000/cores/sf2000-atari800lib", "Atari 800", 0 },
	{ "ATARI7800|A7800", "a78|bin|zip",
		"/mnt/sd/sf2000/cores/sf2000-prosystem", "ProSystem", 0 },
	{ "LYNX", "lnx|zip",
		"/mnt/sd/sf2000/cores/sf2000-handy", "Handy", 0 },
	{ "NGP|NGPC", "ngp|ngc|zip",
		"/mnt/sd/sf2000/cores/sf2000-race", "RACE", 0 },
	{ "WS|WSC|WONDERSWAN", "ws|wsc|zip",
		"/mnt/sd/sf2000/cores/sf2000-beetle-cygne", "Beetle Cygne", 0 },
	{ "COLECO|COLECOVISION", "col|rom|zip",
		"/mnt/sd/sf2000/cores/sf2000-gearcoleco", "Gearcoleco", 0 },
	{ "C64|COMMODORE64", "d64|t64|x64|p00|prg|zip",
		"/mnt/sd/sf2000/cores/sf2000-frodo", "Frodo", 0 },
	{ "PICO8", "p8|png|p8c|zip",
		"/mnt/sd/sf2000/cores/sf2000-fake08", "Fake-08", 0 },
	{ "MSX|MSX1|MSX2", "rom|mx1|mx2|dsk|cas|zip",
		"/mnt/sd/sf2000/cores/sf2000-bluemsx", "blueMSX", 0 },
	{ "JAVASCRIPT|JS2300|CHIP8", "js|mjs|ch8|chip8|zip",
		"/mnt/sd/sf2000/cores/sf2000-js2300-core", "JS2300", 0 },
	{ "SCRIPTS|APPS|JS2300UI", "js|mjs",
		"/usr/bin/sf2000-js2300", "JS2300 UI", 0 },
};

static int list_contains(const char *list, const char *value, size_t length)
{
	while (*list) {
		const char *end = strchr(list, '|');
		size_t item_length = end ? (size_t)(end - list) : strlen(list);

		if (item_length == length && !strncasecmp(list, value, length))
			return 1;
		if (!end)
			break;
		list = end + 1;
	}
	return 0;
}

static int path_has_directory(const char *path, const char *directories)
{
	const char *component = path;

	while ((component = strchr(component, '/'))) {
		const char *end = strchr(++component, '/');
		size_t length = end ? (size_t)(end - component) : strlen(component);

		if (list_contains(directories, component, length))
			return 1;
		if (!end)
			break;
		component = end;
	}
	return 0;
}

static const struct core_route *core_for_path(const char *path)
{
	const char *extension = strrchr(path, '.');
	unsigned i;

	if (!extension || !extension[1])
		return NULL;
	extension++;
	for (i = 0; i < sizeof(core_routes) / sizeof(core_routes[0]); i++)
		if (path_has_directory(path, core_routes[i].directories) &&
				list_contains(core_routes[i].extensions, extension,
					strlen(extension)))
			return &core_routes[i];
	return NULL;
}

static const struct core_route *route_named(const char *name)
{
	unsigned i;

	for (i = 0; i < sizeof(core_routes) / sizeof(core_routes[0]); i++)
		if (!strcmp(core_routes[i].name, name))
			return &core_routes[i];
	return NULL;
}

static const struct core_route *choose_core(int input,
	const struct core_route *fallback, const struct core_route *const *choices,
	unsigned choice_count, const char *family)
{
	unsigned choice = 0;
	unsigned first_choice = 0;
	struct input_event event;
	char message[96];
	unsigned visible;

	if (!choice_count)
		return NULL;
	visible = height > 112u ? (height - 112u) / 45u : 1u;
	if (visible > choice_count)
		visible = choice_count;
	if (!visible)
		visible = 1u;

	for (unsigned i = 0; i < choice_count; i++)
		if (!choices[i]) {
			snprintf(message, sizeof(message), "%s chooser route missing index=%u",
				family, i);
			log_message(message);
			return NULL;
		}

	for (unsigned i = 0; i < choice_count; i++)
		if (choices[i] == fallback)
			choice = i;
	for (;;) {
		unsigned i;
		struct pollfd wait = { .fd = input, .events = POLLIN };

		if (choice < first_choice)
			first_choice = choice;
		if (choice >= first_choice + visible)
			first_choice = choice - visible + 1u;

		sf2000_ui_clear(&ui, ui.config.background);
		sf2000_ui_fill(&ui, 0, 0, (int)width, 42, ui.config.panel);
		sf2000_ui_text(&ui, 13, 11,
			sf2000_ui_label(&ui, SF2000_UI_SELECT_CORE),
			ui.config.header, (int)width - 26);
		for (i = 0; i < visible; i++) {
			unsigned index = first_choice + i;
			int y = 80 + (int)i * 45;
			uint16_t color = index == choice ? ui.config.selected_text :
				ui.config.text;

			if (index == choice)
				sf2000_ui_round(&ui, 22, y - 10, (int)width - 44,
					34, 7, ui.config.accent);
			sf2000_ui_text(&ui, 35, y, choices[index]->name, color,
				(int)width - 70);
		}
		if (first_choice)
			sf2000_ui_text(&ui, (int)width - 30, 55, "^",
			ui.config.muted, 16);
		if (first_choice + visible < choice_count)
			sf2000_ui_text(&ui, (int)width - 30,
				(int)height - 38, "v", ui.config.muted, 16);
		sf2000_ui_text(&ui, 14, (int)height - 22, "A  OK    B  BACK",
			ui.config.muted, (int)width - 28);
		(void)write_frame();
		if (poll(&wait, 1, -1) <= 0)
			return NULL;
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type != EV_KEY || event.value != 1)
				continue;
			if (event.code == BTN_DPAD_UP)
				choice = (choice + choice_count - 1u) % choice_count;
			else if (event.code == BTN_DPAD_DOWN)
				choice = (choice + 1u) % choice_count;
			else if (event.code == BTN_TL)
				choice = menu_move(choice, choice_count, -1);
			else if (event.code == BTN_TR)
				choice = menu_move(choice, choice_count, 1);
			else if (event.code == BTN_EAST)
			{
				snprintf(message, sizeof(message),
					"%s core selected %s", family,
					choices[choice]->name);
				log_message(message);
				return choices[choice];
			}
			else if (event.code == BTN_SOUTH)
				return NULL;
		}
	}
}

static const struct core_route *choose_any_core(int input)
{
	const struct core_route *choices[sizeof(core_routes) /
		(sizeof(core_routes[0]))];
	unsigned count = 0;
	unsigned i;

	for (i = 0; i < sizeof(core_routes) / sizeof(core_routes[0]); i++) {
		unsigned duplicate = 0;
		unsigned j;

		if (access(core_routes[i].executable, X_OK) < 0)
			continue;
		for (j = 0; j < count; j++)
			if (!strcmp(choices[j]->executable,
					core_routes[i].executable)) {
				duplicate = 1;
				break;
			}
		if (!duplicate)
			choices[count++] = &core_routes[i];
	}
	return choose_core(input, NULL, choices, count, "ALL");
}

static const struct core_route *choose_nes_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("FCEUmm"), route_named("QuickNES"),
		route_named("FCEUmm Prosty"), route_named("QuickNES Prosty"),
	};

	return choose_core(input, fallback, choices, 4u, "NES");
}

static const struct core_route *choose_snes_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("Snes9x 2005"), route_named("Snes9x 2002"),
		route_named("Snes9x 2005 Prosty"),
		route_named("Snes9x 2002 Prosty"),
	};

	return choose_core(input, fallback, choices, 4u, "SNES");
}

static const struct core_route *choose_gba_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("gpSP"), route_named("gpSP multicore"),
	};

	return choose_core(input, fallback, choices, 2u, "GBA");
}

static const struct core_route *choose_gb_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("Gambatte"), route_named("Gearboy"),
		route_named("Gambatte Prosty"),
	};

	return choose_core(input, fallback, choices, 3u, "GB");
}

static int run_busybox(char *const argv[])
{
	pid_t child;
	int status;

	child = vfork();
	if (child < 0)
		return -1;
	if (!child) {
		execve(BUSYBOX_PATH, argv, (char *const[]){ NULL });
		_exit(127);
	}
	if (waitpid(child, &status, 0) != child)
		return -1;
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int archive_route_keeps_zip(const struct core_route *route)
{
	/* Arcade cores need the complete set of files in the archive. */
	return route && (!strcmp(route->name, "MAME 2000") ||
		!strcmp(route->name, "FB Alpha 2012"));
}

static int archive_extension_matches(const char *name,
	const struct core_route *route)
{
	const char *dot = strrchr(name, '.');

	return dot && dot[1] && route && strcasecmp(dot + 1, "zip") &&
		list_contains(route->extensions, dot + 1, strlen(dot + 1));
}

static int archive_member_priority(const char *name,
	const struct core_route *route)
{
	const char *dot = strrchr(name, '.');

	if (!archive_extension_matches(name, route))
		return 0;
	if (!strcasecmp(dot + 1, "cue") || !strcasecmp(dot + 1, "m3u"))
		return 3;
	if (!strcasecmp(dot + 1, "chd") || !strcasecmp(dot + 1, "iso") ||
			!strcasecmp(dot + 1, "pbp"))
		return 2;
	return 1;
}

static void scan_archive_directory(const char *directory,
	const struct core_route *route, unsigned depth, char *selected,
	size_t selected_size, int *selected_priority, char *fallback,
	size_t fallback_size)
{
	DIR *dir;
	struct dirent *entry;
	char candidate[MAX_PATH];

	if (depth > 8u)
		return;
	dir = opendir(directory);
	if (!dir)
		return;
	while ((entry = readdir(dir))) {
		struct stat info;
		int priority;

		if (entry->d_name[0] == '.' ||
				snprintf(candidate, sizeof(candidate), "%s/%s",
					directory, entry->d_name) >= (int)sizeof(candidate) ||
				lstat(candidate, &info) < 0)
			continue;
		if (S_ISDIR(info.st_mode)) {
			scan_archive_directory(candidate, route, depth + 1u,
				selected, selected_size, selected_priority, fallback,
				fallback_size);
			continue;
		}
		if (!S_ISREG(info.st_mode))
			continue;
		if (!fallback[0])
			snprintf(fallback, fallback_size, "%s", candidate);
		priority = archive_member_priority(entry->d_name, route);
		if (priority > *selected_priority) {
			snprintf(selected, selected_size, "%s", candidate);
			*selected_priority = priority;
		}
	}
	closedir(dir);
}

static int extract_archive(const char *archive,
	const struct core_route *route, char *output, size_t output_size)
{
	char *const remove_argv[] = {
		(char *)BUSYBOX_PATH, (char *)"rm", (char *)"-rf",
		(char *)ZIP_WORK_DIR, NULL
	};
	char *const mkdir_argv[] = {
		(char *)BUSYBOX_PATH, (char *)"mkdir", (char *)"-p",
		(char *)ZIP_WORK_DIR, NULL
	};
	char *const unzip_argv[] = {
		(char *)BUSYBOX_PATH, (char *)"unzip", (char *)"-o", (char *)"-q",
		(char *)archive, (char *)"-d",
		(char *)ZIP_WORK_DIR, NULL
	};
	char fallback[MAX_PATH] = { 0 };
	int selected_priority = 0;
	unsigned attempt;

	/*
	 * The SD card can deliver a transient short read (busybox unzip dies
	 * with "short read" when a chunk ends before its declared size).  The
	 * archive is removed and re-extracted on every attempt, so a retry is
	 * safe; give the card a brief pause between tries.
	 */
	for (attempt = 0; attempt < 3; attempt++) {
		if (attempt) {
			struct timespec delay = { 0, 100 * 1000 * 1000 };

			(void)nanosleep(&delay, NULL);
		}
		if (run_busybox(remove_argv) == 0 &&
				run_busybox(mkdir_argv) == 0 &&
				run_busybox(unzip_argv) == 0)
			break;
	}
	if (attempt == 3) {
		log_message("archive extraction failed after 3 attempts");
		return -1;
	}
	scan_archive_directory(ZIP_WORK_DIR, route, 0, output, output_size,
		&selected_priority, fallback, sizeof(fallback));
	if (selected_priority)
		return 0;
	if (!fallback[0]) {
		log_message("archive extraction produced no regular ROM member");
		return -1;
	}
	/* A manually selected core is allowed to try an unfamiliar extension. */
	snprintf(output, output_size, "%s", fallback);
	return 0;
}

static int media_path(const char *p)
{
	const char *dot = strrchr(p, '.');
	const char *exts[] = {
		".mp3", ".aac", ".flac", ".wav", ".ogg",
		".mp4", ".mkv", ".avi",
		".png", ".jpg", ".jpeg", ".bmp", ".gif",
	};
	unsigned i;

	if (!dot)
		return 0;
	for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
		if (!strcasecmp(dot, exts[i]))
			return 1;
	}
	return 0;
}

static void launch_selected(int input)
{
	char path[MAX_PATH], launch_path[MAX_PATH], message[640];
	const struct core_route *route;

	if (snprintf(path, sizeof(path), "%s/%s", current, entries[selected].name) >=
			(int)sizeof(path))
		return;
	if (entries[selected].directory) {
		if (entries[selected].target[0])
			snprintf(current, sizeof(current), "%.500s",
				entries[selected].target);
		else
			strcpy(current, path);
		scan_directory();
		return;
	}
	snprintf(launch_path, sizeof(launch_path), "%s", path);
	route = core_for_path(path);
	if (!route) {
		if (media_path(path)) {
			char *const argv[] = { (char *)PLAYER_PATH, path, NULL };
			char *const envp[] = { NULL };

			snprintf(message, sizeof(message), "launch Player %s", path);
			draw_message(SF2000_UI_LOADING, SF2000_UI_ACTIVE,
				"MEDIA", ui.config.header);
			begin_performance_session();
			log_message(message);
			if (sf2000_log_flush("pre-player-launch") != 0)
				log_message("pre-player-launch log flush timed out");
			save_browser_state();
			close_ge_presenter();
			execve(PLAYER_PATH, argv, envp);
			sf2000_performance_end();
			snprintf(message, sizeof(message), "player exec failed errno=%d", errno);
			log_message(message);
			return;
		}
		if (strrchr(path, '.') &&
				!strcasecmp(strrchr(path, '.') + 1, "zip")) {
			route = choose_any_core(input);
			if (!route)
				return;
		} else {
			log_message("unsupported file or directory route; opening core chooser");
			route = choose_any_core(input);
			if (!route)
				return;
		}
	}
	{
		const char *extension = strrchr(path, '.');
		int zip = extension && !strcasecmp(extension, ".zip");

		if (extension && (!strcasecmp(extension, ".nes") ||
				(zip && path_has_directory(path, "NES|FDS")))) {
			route = choose_nes_core(input, route);
			if (!route)
				return;
		} else if (extension &&
				(!strcasecmp(extension, ".sfc") ||
						 !strcasecmp(extension, ".smc") ||
						 (zip && path_has_directory(path, "SNES|SFC|SNES9X2002")))) {
			route = choose_snes_core(input, route);
			if (!route)
				return;
		} else if (extension &&
				(!strcasecmp(extension, ".gb") ||
						 !strcasecmp(extension, ".gbc") ||
						 (zip && path_has_directory(path, "GB|GBC")))) {
			route = choose_gb_core(input, route);
			if (!route)
				return;
		} else if (extension && (!strcasecmp(extension, ".gba") ||
				(zip && path_has_directory(path, "GBA")))) {
			route = choose_gba_core(input, route);
			if (!route)
				return;
		}
		if (zip && !archive_route_keeps_zip(route) &&
				extract_archive(path, route, launch_path,
					sizeof(launch_path)) < 0) {
			draw_message(SF2000_UI_LOADING, SF2000_UI_ACTIVE,
				"ZIP ERROR", 0xf800);
			return;
		}
	}
	{
		if (access(route->executable, X_OK) < 0) {
			snprintf(message, sizeof(message),
				 "missing core %s at %s errno=%d", route->name,
				 route->executable, errno);
			log_message(message);
			draw_message(SF2000_UI_MISSING_CORE, SF2000_UI_INSTALL_CORE,
				route->name, 0xf800);
			return;
		}
		if (route->minimum_mib) {
			struct sysinfo info;
			const unsigned long minimum =
				(unsigned long)route->minimum_mib * 1024ul * 1024ul;

			if (sysinfo(&info) == 0 &&
			    (info.freeram + info.bufferram) * info.mem_unit < minimum) {
				snprintf(message, sizeof(message),
					 "%s needs %u MiB free; available=%lu KiB",
					 route->name, route->minimum_mib,
					 ((info.freeram + info.bufferram) * info.mem_unit) /
					 1024ul);
				log_message(message);
				draw_message(SF2000_UI_NO_MEMORY,
					SF2000_UI_CLOSE_APPS, route->name, 0xf800);
				return;
			}
		}

		snprintf(message, sizeof(message), "launch %s %s%s", route->name,
			path, strcmp(path, launch_path) ? " (extracted)" : "");
		/* Replace the browser before the static-PIE loader allocates and
		 * relocates the core, so the handoff never looks like a frozen
		 * selection screen. */
		draw_message(SF2000_UI_LOADING, SF2000_UI_ACTIVE, route->name,
			ui.config.header);
		log_message(message);
		if (sf2000_log_flush("pre-core-launch") != 0)
			log_message("pre-core-launch log flush timed out");
		save_browser_state();
		close_ge_presenter();
		{
			char *const argv[] = {
				(char *)route->executable, launch_path, NULL
			};
			char *const envp[] = { NULL };

			/*
			 * This is an argv vector, not a shell command.  A ROM name
			 * containing spaces therefore remains exactly one argument.
			 */
			execve(route->executable, argv, envp);
		}
		sf2000_performance_end();
		snprintf(message, sizeof(message), "%s exec failed errno=%d",
			route->name, errno);
	}
	log_message(message);
}

static void parent_directory(void)
{
	char *slash;
	char leaving[MAX_NAME] = { 0 };
	const char *last_slash = strrchr(current, '/');

	if (last_slash && last_slash[1])
		snprintf(leaving, sizeof(leaving), "%s", last_slash + 1);

	if (!strcmp(current, primary_root) || !strcmp(current, SD_ROOT)) {
		view = VIEW_HOME;
		selected = first = 0;
		return;
	}
	/* Leaving an extra volume root returns to the primary card root. */
	if (path_is_extra_root(current)) {
		snprintf(current, sizeof(current), "%s", primary_root);
		scan_directory();
		goto highlight_leaving;
	}
	slash = strrchr(current, '/');
	if (slash && slash > current + strlen(primary_root) - 1u &&
			strncmp(current, primary_root, strlen(primary_root)) == 0 &&
			(current[strlen(primary_root)] == '/' ||
			 current[strlen(primary_root)] == 0)) {
		*slash = 0;
		if (strlen(current) < strlen(primary_root))
			snprintf(current, sizeof(current), "%s", primary_root);
	} else if (slash && slash != current) {
		/* Path under /mnt/sd2/... etc. */
		*slash = 0;
		if (!strchr(current + 1, '/'))
			snprintf(current, sizeof(current), "%s", primary_root);
	} else {
		snprintf(current, sizeof(current), "%s", primary_root);
	}
	scan_directory();

highlight_leaving:
	if (leaving[0]) {
		unsigned i;

		for (i = 0; i < entry_count; i++)
			if (!strcmp(entries[i].name, leaving)) {
				selected = i;
				first = 0;
				break;
			}
	}
}

static void request_system_action(const char *marker, const char *name)
{
	int fd;
	char message[96];

	snprintf(message, sizeof(message), "system action %s", name);
	log_message(message);
	draw_message(SF2000_UI_SAFE_SHUTDOWN, SF2000_UI_ACTIVE, name,
		ui.config.header);
	fd = open(marker, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd >= 0) {
		(void)write(fd, name, strlen(name));
		close(fd);
	}
	for (;;)
		(void)poll(NULL, 0, 1000);
}

int main(void)
{
	struct sf2000_ui_config config;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct input_event event;
	int fb = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	int input;
	if (fb < 0 || ioctl(fb, FBIOGET_FSCREENINFO, &fix) < 0 ||
			ioctl(fb, FBIOGET_VSCREENINFO, &var) < 0 || var.bits_per_pixel != 16)
		return 1;
	width = var.xres; height = var.yres; stride = fix.line_length / 2u;
	if ((size_t)height * stride > MAX_FRAME_PIXELS)
		return 1;
	framebuffer_fd = fb;
	framebuffer_phys = fix.smem_start;
	if (framebuffer_phys && hcge_open_context(&ge_storage) == 0) {
		ge = &ge_storage;
		ge_source = hcge_linux_alloc_buffer(ge,
			(unsigned int)(width * height * sizeof(*ui_pixels)),
			&ge_source_phys, &ge_source_handle);
		if (!ge_source) {
			char message[128];

			snprintf(message, sizeof(message),
				"GE framebuffer source allocation failed bytes=%lu errno=%d",
				(unsigned long)((size_t)width * height *
					sizeof(*ui_pixels)), errno);
			log_message(message);
			close_ge_presenter();
		}
	}
	sf2000_ui_config_defaults(&config);
	(void)sf2000_ui_config_load(&config, "/etc/sf2000.conf");
	(void)sf2000_ui_config_load(&config, "/mnt/sd/sf2000.conf");
	(void)sf2000_ui_init(&ui, ui_pixels, width, height, width, &config);
	{
		char message[320];

		if (ui.font)
			snprintf(message, sizeof(message),
				"font loaded path=%s stb_alloc_failures=%u",
				config.font, sf2000_ui_allocation_failures());
		else
			snprintf(message, sizeof(message),
				"font unavailable path=%s; using fallback glyphs",
				config.font);
		log_message(message);
	}
	input = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (input < 0) {
		sf2000_ui_close(&ui);
		close_ge_presenter();
		return 1;
	}
	load_storage_roots();
	load_browser_state();
	scan_directory();
	selected = first = 0;
	view = VIEW_HOME;
	draw();
	if (launch_startup_game(input)) {
		/* launch_selected() replaces the browser on success.  If exec fails,
		 * leave the user at a usable home screen rather than a half-selected
		 * directory. */
		view = VIEW_HOME;
		snprintf(current, sizeof(current), "%s", primary_root);
		scan_directory();
		selected = first = 0;
		draw();
	}
	{ int ready = open(READY_MARKER, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (ready >= 0) close(ready); }
	{
		char message[160];
		snprintf(message, sizeof(message),
			"ready: home menu A select B back fb=%ux%u stride=%u",
			width, height, stride * 2u);
		log_message(message);
	}
	for (;;) {
		struct pollfd wait = { .fd = input, .events = POLLIN };
		int ready;
		int timeout = (view == VIEW_DEBUG) ? 500 : -1;

		do {
			ready = poll(&wait, 1, timeout);
		} while (ready < 0 && errno == EINTR);
		if (ready < 0)
			break;
		if (ready == 0) {
			if (view == VIEW_DEBUG)
				draw();
			continue;
		}
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			unsigned visible = height > 78 ? (height - 78) / 22u : 1u;
			if (event.type != EV_KEY) continue;
			if (event.code == BTN_START || event.code == BTN_DPAD_RIGHT) {
				unsigned bit = event.code == BTN_START ? 1u : 2u;

				if (event.value)
					log_flush_held |= bit;
				else {
					log_flush_held &= ~bit;
					log_flush_chord_latched = 0;
				}
				if (event.value == 1 && log_flush_held == 3u &&
						!log_flush_chord_latched) {
					log_flush_chord_latched = 1;
					log_message("START+RIGHT log flush requested");
					if (sf2000_log_flush("START+RIGHT") != 0)
						log_message("START+RIGHT log flush timed out");
					else
						log_message("START+RIGHT log flush complete");
					continue;
				}
			}
			if (event.code == BTN_START || event.code == BTN_DPAD_UP) {
				unsigned bit = event.code == BTN_START ? 1u : 2u;

				if (event.value)
					diagnostic_held |= bit;
				else {
					diagnostic_held &= ~bit;
					diagnostic_chord_latched = 0;
				}
				if (event.value == 1 && diagnostic_held == 3u &&
						!diagnostic_chord_latched) {
					diagnostic_chord_latched = 1;
					capture_ui_diagnostic();
					continue;
				}
			}
			if (event.value != 1) continue;
			record_input_event(event.code);
			if (view == VIEW_HOME) {
				if (event.code == BTN_DPAD_UP)
					selected = selected ? selected - 1u : 4u;
				else if (event.code == BTN_DPAD_DOWN && selected < 4u)
					selected++;
				else if (event.code == BTN_TL)
					selected = menu_move(selected, 5u, -1);
				else if (event.code == BTN_TR)
					selected = menu_move(selected, 5u, 1);
				else if (event.code == BTN_EAST) {
					if (selected == 0u) {
						view = VIEW_LIBRARY;
						selected = first = 0;
						load_storage_roots();
						if (!browser_state_valid)
							snprintf(current, sizeof(current),
								"%.500s", primary_root);
						scan_directory();
						restore_browser_selection();
						browser_state_valid = 0;
					} else if (selected == 1u) {
						view = VIEW_SETTINGS;
						selected = 0;
					} else if (selected == 2u) {
						view = VIEW_DEBUG;
						debug_scroll = 0;
					} else if (selected == 3u) {
						request_system_action(RESET_MARKER, "reset\n");
					} else {
						request_system_action(SHUTDOWN_MARKER,
							"shutdown\n");
					}
				}
			} else if (view == VIEW_SETTINGS) {
				if (event.code == BTN_SOUTH) {
					view = VIEW_HOME;
					selected = 1u;
					first = 0;
				}
			} else if (view == VIEW_DEBUG) {
				if (event.code == BTN_SOUTH) {
					view = VIEW_HOME;
					selected = 2u;
					first = 0;
				} else if (event.code == BTN_DPAD_UP) {
					if (debug_scroll > 0)
						debug_scroll--;
				} else if (event.code == BTN_DPAD_DOWN) {
					if (debug_scroll < debug_max_scroll)
						debug_scroll++;
				} else if (event.code == BTN_TL) {
					debug_scroll = debug_scroll > 4u ? debug_scroll - 4u : 0;
				} else if (event.code == BTN_TR) {
					if (debug_scroll + 4u < debug_max_scroll)
						debug_scroll += 4u;
					else
						debug_scroll = debug_max_scroll;
				}
			} else if (event.code == BTN_DPAD_UP) {
				selected = entry_count ?
					(selected ? selected - 1u : entry_count - 1u) : 0u;
			} else if (event.code == BTN_DPAD_DOWN &&
					selected + 1 < entry_count) {
				selected++;
			} else if (event.code == BTN_TL) {
				selected = menu_move(selected, entry_count, -1);
			} else if (event.code == BTN_TR) {
				selected = menu_move(selected, entry_count, 1);
			} else if (event.code == BTN_EAST && entry_count) {
				launch_selected(input);
				/* Drop the chord which quit a core while waitpid() ran. */
				while (read(input, &event, sizeof(event)) == sizeof(event)) { }
			} else if (event.code == BTN_SOUTH) {
				parent_directory();
			}
			if (view == VIEW_LIBRARY) {
				if (selected < first) first = selected;
				if (selected >= first + visible)
					first = selected - visible + 1u;
			}
			draw();
		}
	}
	log_message("returned cleanly");
	sf2000_ui_close(&ui);
	close_ge_presenter();
	close(input); close(fb);
	_exit(0);
}
