// SPDX-License-Identifier: MIT

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
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "ge_api.h"
#include "sf2000_browser_ui.h"
#include "sf2000_log.h"

extern long syscall(long number, ...);

#define READY_MARKER "/run/sf2000-frontend-ready"
#define PERFORMANCE_MARKER "/run/sf2000-performance-active"
#define PERFORMANCE_READY_MARKER "/run/sf2000-performance-ready"
#define RESET_MARKER "/run/sf2000-reboot-request"
#define SHUTDOWN_MARKER "/run/sf2000-shutdown-request"
#define GAMBATTE_PATH "/usr/bin/sf2000-gambatte"
#define GPSP_PATH "/usr/bin/sf2000-gpsp"
#define FCEUMM_PATH "/usr/bin/sf2000-fceumm"
#define PLAYER_PATH "/usr/bin/sf2000-player"
#define SD_ROOT "/mnt/sd"
#define STORAGE_ROOTS_PATH "/run/sf2000-storage-roots"
#define MAX_ENTRIES 128
#define MAX_EXTRA_ROOTS 8u
#define MAX_NAME 128
#define MAX_PATH 512
#define MAX_FRAME_PIXELS (320u * 240u)
#define LINUX_DT_DIR 4u
#define LINUX_DT_REG 8u
#define UI_DIAGNOSTIC_PATH SD_ROOT "/sf2000/ui-diagnostic.bin"
#define UI_DIAGNOSTIC_MAGIC "SF2KUID1"

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
static uint16_t framebuffer[MAX_FRAME_PIXELS];
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
static char primary_root[MAX_PATH] = SD_ROOT;
static char extra_roots[MAX_EXTRA_ROOTS][MAX_PATH];
static char extra_labels[MAX_EXTRA_ROOTS][MAX_NAME];
static unsigned extra_root_count;
static struct sf2000_ui ui;
enum browser_view { VIEW_HOME, VIEW_LIBRARY, VIEW_SETTINGS };
static enum browser_view view = VIEW_HOME;
static unsigned framebuffer_writes;
static unsigned diagnostic_chord_latched;
static unsigned diagnostic_held;
static unsigned log_flush_chord_latched;
static unsigned log_flush_held;
static void log_message(const char *message);
static int write_frame(void);

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

static void begin_performance_session(void)
{
	unsigned attempt;
	int marker = open(PERFORMANCE_MARKER,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

	if (marker >= 0)
		close(marker);
	for (attempt = 0; attempt < 100u &&
			access(PERFORMANCE_READY_MARKER, F_OK) != 0; attempt++)
		(void)poll(NULL, 0, 10);
	if (access(PERFORMANCE_READY_MARKER, F_OK) != 0)
		log_message("performance journal acknowledgement timeout");
}

static ssize_t write_cpu_frame(void)
{
	size_t bytes = (size_t)height * stride * sizeof(*framebuffer);

	return pwrite(framebuffer_fd, framebuffer, bytes, 0);
}

static uint32_t diagnostic_hash(const uint16_t *pixels)
{
	uint32_t hash = 2166136261u;
	unsigned y, x;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++) {
			hash ^= pixels[y * stride + x];
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
			if (pixels[y * stride + x] != framebuffer[y * stride + x])
				mismatches++;
	return mismatches;
}

static int diagnostic_readback(uint16_t *destination)
{
	size_t bytes = (size_t)height * stride * sizeof(*framebuffer);
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

static int diagnostic_write_packed(int fd, const uint16_t *pixels)
{
	unsigned y;

	for (y = 0; y < height; y++)
		if (diagnostic_write_all(fd, pixels + y * stride,
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
	ge_hash = diagnostic_hash(diagnostic_ge_scanout);
	ge_mismatches = diagnostic_mismatches(diagnostic_ge_scanout);

	/* A CPU publication of the identical source isolates the GE transfer. */
	if (write_cpu_frame() != (ssize_t)((size_t)height * stride *
		sizeof(*framebuffer)) || diagnostic_readback(diagnostic_scanout) < 0)
		goto restore;
	cpu_hash = diagnostic_hash(diagnostic_scanout);
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
	header.source_hash = diagnostic_hash(framebuffer);
	header.ge_hash = ge_hash;
	header.cpu_hash = cpu_hash;
	header.ge_mismatches = ge_mismatches;
	header.cpu_mismatches = cpu_mismatches;
	header.frame_bytes = (uint32_t)((size_t)width * height *
		sizeof(*framebuffer));
	fd = open(UI_DIAGNOSTIC_PATH,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0 || diagnostic_write_all(fd, &header, sizeof(header)) < 0 ||
		diagnostic_write_packed(fd, framebuffer) < 0 ||
		diagnostic_write_packed(fd, diagnostic_ge_scanout) < 0) {
		snprintf(message, sizeof(message),
			"UI diagnostic write failed path=%s errno=%d",
			UI_DIAGNOSTIC_PATH, errno);
		log_message(message);
		if (fd >= 0)
			close(fd);
		return;
	}
	/* The final packed image is the CPU-publication readback. */
	if (diagnostic_write_packed(fd, diagnostic_scanout) < 0 || fsync(fd) < 0) {
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
	size_t bytes = (size_t)height * stride * sizeof(*framebuffer);
	ssize_t written;
	const char *presenter = "CPU";
	int ge_presented = 0;
	char message[160];

	if (ge) {
		unsigned y;
		size_t row_bytes = (size_t)width * sizeof(*framebuffer);
		hcge_state *state = &ge->state;
		HCGERectangle source = { 0, 0, (int)width, (int)height };

		if (stride == width)
			memcpy(ge_source, framebuffer, bytes);
		else
			for (y = 0; y < height; y++)
				memcpy(ge_source + y * width,
					framebuffer + y * stride, row_bytes);
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
			state->dst.pitch = stride * sizeof(*framebuffer);
			state->src.phys = ge_source_phys;
			state->src.pitch = width * sizeof(*framebuffer);
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
			(unsigned long)bytes, stride * (unsigned)sizeof(*framebuffer),
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

static void draw_library(void)
{
	unsigned row, visible = height > 78 ? (height - 78) / 22u : 1u;
	char line[MAX_NAME + 8];
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
		snprintf(line, sizeof(line), "%s  %s",
			entries[index].directory ? "\xe2\x96\xb8" : "\xc2\xb7",
			entries[index].name);
		sf2000_ui_text(&ui, 14, y, line, color, (int)width - 28);
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
		SF2000_UI_LIBRARY, SF2000_UI_SETTINGS, SF2000_UI_RESET,
		SF2000_UI_SAFE_SHUTDOWN,
	};
	unsigned i;

	sf2000_ui_clear(&ui, ui.config.background);
	sf2000_ui_fill(&ui, 0, 0, (int)width, 42, ui.config.panel);
	sf2000_ui_text(&ui, 13, 11, "SF2000 LINUX", ui.config.header,
		(int)width - 26);
	for (i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		int y = 60 + (int)i * 38;
		uint16_t color = i == selected ? ui.config.selected_text :
			ui.config.text;

		if (i == selected)
			sf2000_ui_round(&ui, 16, y - 8, (int)width - 32, 31, 7,
				ui.config.accent);
		sf2000_ui_text(&ui, 27, y,
			sf2000_ui_label(&ui, items[i]), color, (int)width - 54);
	}
	sf2000_ui_text(&ui, 14, (int)height - 22, "A  OK",
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

static void draw(void)
{
	if (view == VIEW_HOME)
		draw_home();
	else if (view == VIEW_SETTINGS)
		draw_settings();
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
	{ "QUICKNES", "nes",
		"/mnt/sd/sf2000/cores/sf2000-quicknes", "QuickNES", 0 },
	{ "GB|GBC", "gb|gbc", GAMBATTE_PATH, "Gambatte", 0 },
	{ "GB|GBC", "gb|gbc",
		"/mnt/sd/sf2000/cores/sf2000-gearboy", "Gearboy", 0 },
	{ "GBA", "gba", GPSP_PATH, "gpSP", 40 },
	{ "NES|FDS", "nes|fds", FCEUMM_PATH, "FCEUmm", 0 },
	{ "MD|GENESIS|MEGADRIVE|SMS|GG|32X",
		"md|gen|smd|sms|gg|sg|32x|cue|chd|iso",
		"/mnt/sd/sf2000/cores/sf2000-picodrive", "PicoDrive", 0 },
	{ "SNES|SFC", "sfc|smc",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2005", "Snes9x 2005", 0 },
	{ "SNES9X2002", "sfc|smc",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2002", "Snes9x 2002", 0 },
	{ "PCE|PCENGINE|SGX", "pce|sgx|cue|chd",
		"/mnt/sd/sf2000/cores/sf2000-pce-fast", "PCE Fast", 0 },
	{ "PS|PSX|PLAYSTATION", "bin|iso|img|cue|pbp",
		"/mnt/sd/sf2000/cores/sf2000-qpsx", "QPSX", 48 },
	{ "ARCADE|MAME", "zip",
		"/mnt/sd/sf2000/cores/sf2000-mame2000", "MAME 2000", 0 },
	{ "FBNEO|FBA", "zip",
		"/mnt/sd/sf2000/cores/sf2000-fbalpha2012", "FB Alpha 2012", 0 },
	{ "ATARI2600|A2600", "a26|bin",
		"/mnt/sd/sf2000/cores/sf2000-stella2014", "Stella 2014", 0 },
	{ "ATARI5200|A5200", "a52|bin",
		"/mnt/sd/sf2000/cores/sf2000-a5200", "A5200", 0 },
	{ "ATARI7800|A7800", "a78|bin",
		"/mnt/sd/sf2000/cores/sf2000-prosystem", "ProSystem", 0 },
	{ "LYNX", "lnx",
		"/mnt/sd/sf2000/cores/sf2000-handy", "Handy", 0 },
	{ "NGP|NGPC", "ngp|ngc",
		"/mnt/sd/sf2000/cores/sf2000-race", "RACE", 0 },
	{ "WS|WSC|WONDERSWAN", "ws|wsc",
		"/mnt/sd/sf2000/cores/sf2000-beetle-cygne", "Beetle Cygne", 0 },
	{ "COLECO|COLECOVISION", "col|rom",
		"/mnt/sd/sf2000/cores/sf2000-gearcoleco", "Gearcoleco", 0 },
	{ "C64|COMMODORE64", "d64|t64|x64|p00|prg",
		"/mnt/sd/sf2000/cores/sf2000-frodo", "Frodo", 0 },
	{ "PICO8", "p8",
		"/mnt/sd/sf2000/cores/sf2000-fake08", "Fake-08", 0 },
	{ "MSX", "rom|mx1|mx2|dsk|cas",
		"/mnt/sd/sf2000/cores/sf2000-bluemsx", "blueMSX", 0 },
	{ "JAVASCRIPT|JS2300|CHIP8", "js|mjs|ch8|chip8",
		"/mnt/sd/sf2000/cores/sf2000-js2300", "JS2300", 0 },
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
	struct input_event event;
	char message[96];

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

		sf2000_ui_clear(&ui, ui.config.background);
		sf2000_ui_fill(&ui, 0, 0, (int)width, 42, ui.config.panel);
		sf2000_ui_text(&ui, 13, 11,
			sf2000_ui_label(&ui, SF2000_UI_SELECT_CORE),
			ui.config.header, (int)width - 26);
		for (i = 0; i < choice_count; i++) {
			int y = 80 + (int)i * 45;
			uint16_t color = i == choice ? ui.config.selected_text :
				ui.config.text;

			if (i == choice)
				sf2000_ui_round(&ui, 22, y - 10, (int)width - 44,
					34, 7, ui.config.accent);
			sf2000_ui_text(&ui, 35, y, choices[i]->name, color,
				(int)width - 70);
		}
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

static const struct core_route *choose_nes_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("FCEUmm"), route_named("QuickNES"),
	};

	return choose_core(input, fallback, choices, 2u, "NES");
}

static const struct core_route *choose_snes_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("Snes9x 2005"), route_named("Snes9x 2002"),
	};

	return choose_core(input, fallback, choices, 2u, "SNES");
}

static const struct core_route *choose_gb_core(int input,
	const struct core_route *fallback)
{
	const struct core_route *choices[] = {
		route_named("Gambatte"), route_named("Gearboy"),
	};

	return choose_core(input, fallback, choices, 2u, "GB");
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
	char path[MAX_PATH], message[640];
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
			close_ge_presenter();
			execve(PLAYER_PATH, argv, envp);
			(void)unlink(PERFORMANCE_MARKER);
			snprintf(message, sizeof(message), "player exec failed errno=%d", errno);
			log_message(message);
			return;
		}
		log_message("unsupported file or directory route");
		return;
	}
	{
		const char *extension = strrchr(path, '.');

		if (extension && !strcasecmp(extension, ".nes")) {
			route = choose_nes_core(input, route);
			if (!route)
				return;
		} else if (extension &&
				(!strcasecmp(extension, ".sfc") ||
					 !strcasecmp(extension, ".smc"))) {
			route = choose_snes_core(input, route);
			if (!route)
				return;
		} else if (extension &&
				(!strcasecmp(extension, ".gb") ||
					 !strcasecmp(extension, ".gbc"))) {
			route = choose_gb_core(input, route);
			if (!route)
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

		snprintf(message, sizeof(message), "launch %s %s", route->name, path);
		/* Replace the browser before the static-PIE loader allocates and
		 * relocates the core, so the handoff never looks like a frozen
		 * selection screen. */
		draw_message(SF2000_UI_LOADING, SF2000_UI_ACTIVE, route->name,
			ui.config.header);
		begin_performance_session();
		log_message(message);
		if (sf2000_log_flush("pre-core-launch") != 0)
			log_message("pre-core-launch log flush timed out");
		close_ge_presenter();
		{
			char *const argv[] = {
				(char *)route->executable, path, NULL
			};
			char *const envp[] = { NULL };

			/*
			 * This is an argv vector, not a shell command.  A ROM name
			 * containing spaces therefore remains exactly one argument.
			 */
			execve(route->executable, argv, envp);
		}
		(void)unlink(PERFORMANCE_MARKER);
		snprintf(message, sizeof(message), "%s exec failed errno=%d",
			route->name, errno);
	}
	log_message(message);
}

static void parent_directory(void)
{
	char *slash;

	if (!strcmp(current, primary_root) || !strcmp(current, SD_ROOT)) {
		view = VIEW_HOME;
		selected = first = 0;
		return;
	}
	/* Leaving an extra volume root returns to the primary card root. */
	if (path_is_extra_root(current)) {
		snprintf(current, sizeof(current), "%s", primary_root);
		scan_directory();
		return;
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
			(unsigned int)(width * height * sizeof(*framebuffer)),
			&ge_source_phys, &ge_source_handle);
		if (!ge_source) {
			char message[128];

			snprintf(message, sizeof(message),
				"GE framebuffer source allocation failed bytes=%lu errno=%d",
				(unsigned long)((size_t)width * height *
					sizeof(*framebuffer)), errno);
			log_message(message);
			close_ge_presenter();
		}
	}
	sf2000_ui_config_defaults(&config);
	(void)sf2000_ui_config_load(&config, "/etc/sf2000.conf");
	(void)sf2000_ui_config_load(&config, "/mnt/sd/sf2000.conf");
	(void)sf2000_ui_init(&ui, framebuffer, width, height, stride, &config);
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
	scan_directory();
	selected = first = 0;
	view = VIEW_HOME;
	draw();
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

		do {
			ready = poll(&wait, 1, -1);
		} while (ready < 0 && errno == EINTR);
		if (ready <= 0)
			break;
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
			if (view == VIEW_HOME) {
				if (event.code == BTN_DPAD_UP && selected)
					selected--;
				else if (event.code == BTN_DPAD_DOWN && selected < 3u)
					selected++;
				else if (event.code == BTN_EAST) {
					if (selected == 0u) {
						view = VIEW_LIBRARY;
						selected = first = 0;
						load_storage_roots();
						snprintf(current, sizeof(current),
							"%.500s", primary_root);
						scan_directory();
					} else if (selected == 1u) {
						view = VIEW_SETTINGS;
						selected = 0;
					} else if (selected == 2u) {
						request_system_action(RESET_MARKER, "reset\n");
					} else {
						request_system_action(SHUTDOWN_MARKER,
							"shutdown\n");
					}
				}
			} else if (view == VIEW_SETTINGS) {
				if (event.code == BTN_SOUTH) {
					view = VIEW_HOME;
					selected = first = 0;
				}
			} else if (event.code == BTN_DPAD_UP && selected) {
				selected--;
			} else if (event.code == BTN_DPAD_DOWN &&
					selected + 1 < entry_count) {
				selected++;
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
