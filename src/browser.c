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

#include "sf2000_browser_ui.h"

extern long syscall(long number, ...);

#define READY_MARKER "/run/sf2000-frontend-ready"
#define EXIT_MARKER "/run/sf2000-browser-exit"
#define GAMBATTE_PATH "/usr/bin/sf2000-gambatte"
#define GPSP_PATH "/usr/bin/sf2000-gpsp"
#define FCEUMM_PATH "/usr/bin/sf2000-fceumm"
#define PLAYER_PATH "/usr/bin/sf2000-player"
#define SD_ROOT "/mnt/sd"
#define MAX_ENTRIES 128
#define MAX_NAME 128
#define MAX_PATH 512
#define MAX_FRAME_PIXELS (320u * 240u)
#define LINUX_DT_DIR 4u
#define LINUX_DT_REG 8u

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

struct entry { char name[MAX_NAME]; unsigned directory; };
static struct entry entries[MAX_ENTRIES];
static unsigned entry_count, selected, first;
static uint16_t framebuffer[MAX_FRAME_PIXELS];
static int framebuffer_fd = -1;
static unsigned width, height, stride;
static char current[MAX_PATH] = SD_ROOT;
static struct sf2000_ui ui;

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
			entry_count++;
		}
	}
	close(directory);
	qsort(entries, entry_count, sizeof(entries[0]), compare_entries);
	{
		char message[640];
		snprintf(message, sizeof(message), "directory path=%s entries=%u",
			current, entry_count);
		log_message(message);
	}
}

static void draw(void)
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
	snprintf(footer, sizeof(footer), "A %s   B %s   START+L %s",
		sf2000_ui_label(&ui, SF2000_UI_OPEN),
		sf2000_ui_label(&ui, SF2000_UI_BACK),
		sf2000_ui_label(&ui, SF2000_UI_EXIT));
	sf2000_ui_text(&ui, 10, (int)height - 22, footer,
		ui.config.muted, (int)width - 20);
	if (pwrite(framebuffer_fd, framebuffer,
			(size_t)height * stride * sizeof(*framebuffer), 0) < 0)
		log_message("framebuffer write failed");
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
	if (pwrite(framebuffer_fd, framebuffer,
			(size_t)height * stride * sizeof(*framebuffer), 0) < 0)
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
	{ "GB|GBC", "gb|gbc", GAMBATTE_PATH, "Gambatte", 0 },
	{ "GBA", "gba", GPSP_PATH, "gpSP", 40 },
	{ "NES|FDS", "nes|fds", FCEUMM_PATH, "FCEUmm", 0 },
	{ "MD|GENESIS|MEGADRIVE|SMS|GG|32X",
		"md|gen|smd|sms|gg|sg|32x|cue|chd|iso",
		"/mnt/sd/sf2000/cores/sf2000-picodrive", "PicoDrive", 0 },
	{ "SNES|SFC", "sfc|smc",
		"/mnt/sd/sf2000/cores/sf2000-snes9x2005", "Snes9x 2005", 0 },
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

static void launch_selected(void)
{
	char path[MAX_PATH], message[640];
	const struct core_route *route;

	if (snprintf(path, sizeof(path), "%s/%s", current, entries[selected].name) >=
			(int)sizeof(path))
		return;
	if (entries[selected].directory) {
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
			log_message(message);
			draw_message(SF2000_UI_LOADING, SF2000_UI_ACTIVE,
				"MEDIA", ui.config.header);
			execve(PLAYER_PATH, argv, envp);
			snprintf(message, sizeof(message), "player exec failed errno=%d", errno);
			log_message(message);
			return;
		}
		log_message("unsupported file or directory route");
		return;
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
		log_message(message);
		/* Replace the browser before the static-PIE loader allocates and
		 * relocates the core, so the handoff never looks like a frozen
		 * selection screen. */
		draw_message(SF2000_UI_LOADING, SF2000_UI_ACTIVE, route->name,
			ui.config.header);
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
		snprintf(message, sizeof(message), "%s exec failed errno=%d",
			route->name, errno);
	}
	log_message(message);
}

static void parent_directory(void)
{
	char *slash;
	if (!strcmp(current, SD_ROOT)) return;
	slash = strrchr(current, '/');
	if (slash && slash > current + strlen(SD_ROOT) - 1u) *slash = 0;
	else strcpy(current, SD_ROOT);
	scan_directory();
}

int main(void)
{
	struct sf2000_ui_config config;
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct input_event event;
	int fb = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	int input, start = 0, l = 0, exit_latched = 0;
	if (fb < 0 || ioctl(fb, FBIOGET_FSCREENINFO, &fix) < 0 ||
			ioctl(fb, FBIOGET_VSCREENINFO, &var) < 0 || var.bits_per_pixel != 16)
		return 1;
	width = var.xres; height = var.yres; stride = fix.line_length / 2u;
	if ((size_t)height * stride > MAX_FRAME_PIXELS)
		return 1;
	framebuffer_fd = fb;
	sf2000_ui_config_defaults(&config);
	(void)sf2000_ui_config_load(&config, "/etc/sf2000.conf");
	(void)sf2000_ui_config_load(&config, "/mnt/sd/sf2000.conf");
	(void)sf2000_ui_init(&ui, framebuffer, width, height, stride, &config);
	input = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (input < 0) {
		sf2000_ui_close(&ui);
		return 1;
	}
	scan_directory(); draw();
	{ int ready = open(READY_MARKER, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
		if (ready >= 0) close(ready); }
	{
		char message[160];
		snprintf(message, sizeof(message),
			"ready: DPAD select A open B back START+L exit fb=%ux%u stride=%u",
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
			if (event.code == BTN_START || event.code == BTN_TL) {
				if (event.code == BTN_START) start = event.value != 0;
				else l = event.value != 0;
				if (!start && !l) exit_latched = 0;
				if (!exit_latched && start && l) {
					exit_latched = 1;
					goto done;
				}
				continue;
			}
			if (event.value != 1) continue;
			if (event.code == BTN_DPAD_UP && selected) selected--;
			else if (event.code == BTN_DPAD_DOWN && selected + 1 < entry_count) selected++;
			else if (event.code == BTN_EAST && entry_count) {
				launch_selected();
				/* Drop the chord which quit a core while waitpid() ran. */
				while (read(input, &event, sizeof(event)) == sizeof(event)) { }
				start = l = exit_latched = 0;
			}
			else if (event.code == BTN_SOUTH) parent_directory();
			if (selected < first) first = selected;
			if (selected >= first + visible) first = selected - visible + 1u;
			draw();
		}
	}
done:
	log_message("returned cleanly");
	{
		int marker = open(EXIT_MARKER,
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);

		if (marker >= 0)
			close(marker);
	}
	sf2000_ui_close(&ui);
	close(input); close(fb);
	_exit(0);
}
