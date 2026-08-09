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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "ge_api.h"
#include "sf2000_browser_ui.h"
#include "sf2000_input.h"
#include "sf2000_log.h"
#include <js2300/js2300.h>

#define JS2300_MAX_PATH 512u
#define JS2300_MAX_FRAME_PIXELS (320u * 240u)
#define JS2300_HEAP_BYTES (8u * 1024u * 1024u)

struct js2300_runner {
	int background;
	int exit_requested;
	int fb_fd;
	int input_open;
	unsigned width;
	unsigned height;
	unsigned stride;
	uint32_t framebuffer_phys;
	uint16_t *pixels;
	struct sf2000_ui ui;
	struct sf2000_input input;
	hcge_context ge_storage;
	hcge_context *ge;
	uint16_t *ge_source;
	uint32_t ge_source_phys;
	uint32_t ge_source_handle;
};

static void runner_log(const char *message)
{
	char line[640];
	int fd;
	int length;

	fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line), "<6>sf2000-js2300: %s\n",
		message ? message : "");
	if (length > 0)
		(void)write(fd, line, (size_t)length);
	close(fd);
}

static uint32_t runner_millis(void *opaque)
{
	struct timeval tv;

	(void)opaque;
	if (gettimeofday(&tv, NULL) != 0)
		return 0;
	return (uint32_t)((uint32_t)tv.tv_sec * 1000u +
		(uint32_t)(tv.tv_usec / 1000u));
}

static void runner_sleep(void *opaque, uint32_t milliseconds)
{
	(void)opaque;
	(void)poll(NULL, 0, (int)milliseconds);
}

static void runner_host_log(void *opaque, const char *message)
{
	(void)opaque;
	runner_log(message);
}

static int runner_flush_log(void *opaque)
{
	(void)opaque;
	return sf2000_log_flush("js2300");
}

static int runner_ensure_ui(struct js2300_runner *runner)
{
	if (!runner || runner->background)
		return -1;
	return runner->pixels ? 0 : -1;
}

static void runner_video_clear(void *opaque, uint16_t color)
{
	struct js2300_runner *runner = opaque;

	if (runner_ensure_ui(runner) != 0)
		return;
	sf2000_ui_clear(&runner->ui, color);
}

static void runner_video_rects(void *opaque, const struct js2300_rect *rects,
	size_t count)
{
	struct js2300_runner *runner = opaque;

	if (runner_ensure_ui(runner) != 0 || !rects)
		return;
	for (size_t i = 0; i < count; i++)
		sf2000_ui_fill(&runner->ui, rects[i].x, rects[i].y, rects[i].w,
			rects[i].h, rects[i].color);
}

static void runner_video_text(void *opaque, int x, int y, const char *text,
	uint16_t color)
{
	struct js2300_runner *runner = opaque;

	if (runner_ensure_ui(runner) != 0 || !text)
		return;
	(void)sf2000_ui_text(&runner->ui, x, y, text, color,
		(int)runner->width - x);
}

static int runner_video_image(void *opaque, const char *path, int x, int y,
	int w, int h)
{
	(void)opaque;
	(void)path;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	return -1;
}

static int runner_write_cpu_frame(struct js2300_runner *runner)
{
	size_t row_bytes = (size_t)runner->width * sizeof(*runner->pixels);
	size_t bytes = (size_t)runner->height * runner->stride *
		sizeof(*runner->pixels);

	if (runner->stride == runner->width)
		return pwrite(runner->fb_fd, runner->pixels, bytes, 0) ==
			(ssize_t)bytes ? 0 : -1;
	for (unsigned y = 0; y < runner->height; y++)
		if (pwrite(runner->fb_fd, runner->pixels + y * runner->width,
			row_bytes, (off_t)y * runner->stride * sizeof(*runner->pixels)) !=
			(ssize_t)row_bytes)
			return -1;
	return 0;
}

static int runner_present_ge(struct js2300_runner *runner)
{
	hcge_state *state;
	HCGERectangle source = { 0, 0, 0, 0 };
	size_t row_bytes;

	if (!runner->ge)
		return -1;
	row_bytes = (size_t)runner->width * sizeof(*runner->pixels);
	if (runner->stride == runner->width)
		memcpy(runner->ge_source, runner->pixels, row_bytes * runner->height);
	else
		for (unsigned y = 0; y < runner->height; y++)
			memcpy(runner->ge_source + y * runner->width,
				runner->pixels + y * runner->width, row_bytes);
	if (hcge_linux_cache_clean(runner->ge, runner->ge_source,
			(unsigned int)(row_bytes * runner->height)) != 0)
		return -1;
	state = &runner->ge->state;
	memset(state, 0, sizeof(*state));
	state->render_options = HCGE_DSRO_NONE;
	state->drawingflags = HCGE_DSDRAW_NOFX;
	state->blittingflags = HCGE_DSBLIT_NOFX;
	state->destination.config.format = HCGE_DSPF_RGB16;
	state->destination.config.size.w = (int)runner->width;
	state->destination.config.size.h = (int)runner->height;
	state->source.config.format = HCGE_DSPF_RGB16;
	state->source.config.size.w = (int)runner->width;
	state->source.config.size.h = (int)runner->height;
	state->dst.phys = runner->framebuffer_phys;
	state->dst.pitch = runner->stride * sizeof(*runner->pixels);
	state->src.phys = runner->ge_source_phys;
	state->src.pitch = runner->width * sizeof(*runner->pixels);
	state->accel = HCGE_DFXL_BLIT;
	source.w = (int)runner->width;
	source.h = (int)runner->height;
	hcge_set_state(runner->ge, state, state->accel);
	return hcge_blit(runner->ge, &source, 0, 0) &&
		hcge_engine_sync(runner->ge) == 0 ? 0 : -1;
}

static void runner_video_present(void *opaque)
{
	struct js2300_runner *runner = opaque;

	if (runner_ensure_ui(runner) != 0)
		return;
	if (runner_present_ge(runner) != 0 && runner_write_cpu_frame(runner) != 0)
		runner_log("frame present failed");
}

static uint32_t runner_input_poll(void *opaque)
{
	struct js2300_runner *runner = opaque;

	if (!runner || !runner->input_open)
		return 0;
	(void)sf2000_input_poll(&runner->input);
	return runner->input.keys;
}

static void runner_battery(void *opaque, struct js2300_battery_status *status)
{
	(void)opaque;
	if (!status)
		return;
	status->percent = -1;
	status->charging = 0;
	status->low = 0;
}

static int runner_backlight(void *opaque, int level, int *out_level)
{
	(void)opaque;
	(void)level;
	if (out_level)
		*out_level = -1;
	return -1;
}

static int runner_av_output(void *opaque, int mode, int *out_mode)
{
	(void)opaque;
	(void)mode;
	if (out_mode)
		*out_mode = -1;
	return -1;
}

static int runner_path(const char *path, char *out, size_t out_size)
{
	if (!path || !path[0] || !out || out_size == 0 || path[0] != '/' ||
		strlen(path) >= out_size)
		return -1;
	memcpy(out, path, strlen(path) + 1u);
	return 0;
}

static int runner_fs_list(void *opaque, const char *path,
	struct js2300_fs_entry *entries, size_t max_entries)
{
	DIR *directory;
	struct dirent *entry;
	int count = 0;
	char full[JS2300_MAX_PATH];
	struct stat st;

	(void)opaque;
	if (!path || !entries || !max_entries || (directory = opendir(path)) == NULL)
		return -1;
	while (count < (int)max_entries && (entry = readdir(directory)) != NULL) {
		if (entry->d_name[0] == '.' || strlen(entry->d_name) >=
			sizeof(entries[count].name))
			continue;
		memset(&entries[count], 0, sizeof(entries[count]));
		memcpy(entries[count].name, entry->d_name,
			strlen(entry->d_name) + 1u);
		if (snprintf(full, sizeof(full), "%s/%s", path, entry->d_name) <
			(int)sizeof(full) && stat(full, &st) == 0)
			entries[count].is_dir = S_ISDIR(st.st_mode) ? 1u : 0u;
		count++;
	}
	closedir(directory);
	return count;
}

static int runner_fs_read_bytes(void *opaque, const char *path, uint8_t *out,
	size_t out_size)
{
	char checked_path[JS2300_MAX_PATH];
	FILE *file;
	size_t got;
	int error;

	(void)opaque;
	if (runner_path(path, checked_path, sizeof(checked_path)) != 0 ||
		!out || !out_size || (file = fopen(checked_path, "rb")) == NULL)
		return -1;
	got = fread(out, 1, out_size, file);
	error = ferror(file);
	fclose(file);
	return error ? -1 : (int)got;
}

static int runner_fs_read_text(void *opaque, const char *path, char *out,
	size_t out_size)
{
	int got;

	if (!out || out_size < 2u)
		return -1;
	got = runner_fs_read_bytes(opaque, path, (uint8_t *)out, out_size - 1u);
	if (got < 0)
		return got;
	out[got] = '\0';
	return got;
}

static int runner_fs_write_bytes(void *opaque, const char *path,
	const uint8_t *data, size_t size)
{
	char destination[JS2300_MAX_PATH];
	char temporary[JS2300_MAX_PATH];
	FILE *file;
	int write_ok;
	int close_ok;
	int ret = -1;

	(void)opaque;
	if (runner_path(path, destination, sizeof(destination)) != 0 || !data ||
		snprintf(temporary, sizeof(temporary), "%s.tmp", destination) >=
		(int)sizeof(temporary) || (file = fopen(temporary, "wb")) == NULL)
		return -1;
	write_ok = (!size || fwrite(data, 1, size, file) == size) &&
		fflush(file) == 0;
	close_ok = fclose(file) == 0;
	if (write_ok && close_ok && rename(temporary, destination) == 0)
		ret = 0;
	if (ret != 0)
		(void)unlink(temporary);
	return ret;
}

static int runner_fs_write_text(void *opaque, const char *path,
	const char *text, size_t size)
{
	return runner_fs_write_bytes(opaque, path, (const uint8_t *)text, size);
}

static int runner_action(void *opaque, const char *id)
{
	(void)opaque;
	if (id && !strncmp(id, "toast:", 6)) {
		runner_log(id);
		return 1;
	}
	return -1;
}

static void runner_exit(void *opaque, const char *reason)
{
	struct js2300_runner *runner = opaque;

	if (runner)
		runner->exit_requested = 1;
	runner_log(reason ? reason : "script exit");
}

static void runner_configure_host(struct js2300_runner *runner,
	struct js2300_host *host)
{
	memset(host, 0, sizeof(*host));
	host->size = sizeof(*host);
	host->opaque = runner;
	host->log = runner_host_log;
	host->flush_log = runner_flush_log;
	host->millis = runner_millis;
	host->sleep_ms = runner_sleep;
	host->video_clear = runner_video_clear;
	host->video_rects = runner_video_rects;
	host->video_text = runner_video_text;
	host->video_image = runner_video_image;
	host->video_present = runner_video_present;
	host->input_poll = runner_input_poll;
	host->battery = runner_battery;
	host->fs_list = runner_fs_list;
	host->action = runner_action;
	host->exit = runner_exit;
	host->backlight = runner_backlight;
	host->av_output = runner_av_output;
	host->fs_read_text = runner_fs_read_text;
	host->fs_write_text = runner_fs_write_text;
}

static int runner_open_ui(struct js2300_runner *runner)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;
	struct sf2000_ui_config config;

	runner->fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (runner->fb_fd < 0 || ioctl(runner->fb_fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
		ioctl(runner->fb_fd, FBIOGET_VSCREENINFO, &var) < 0 ||
		var.bits_per_pixel != 16 || var.xres * var.yres > JS2300_MAX_FRAME_PIXELS)
		return -1;
	runner->width = var.xres;
	runner->height = var.yres;
	runner->stride = fix.line_length / 2u;
	runner->framebuffer_phys = fix.smem_start;
	runner->pixels = calloc(runner->width * runner->height, sizeof(*runner->pixels));
	if (!runner->pixels)
		return -1;
	sf2000_ui_config_defaults(&config);
	(void)sf2000_ui_config_load(&config, "/etc/sf2000.conf");
	(void)sf2000_ui_config_load(&config, "/mnt/sd/sf2000.conf");
	if (sf2000_ui_init(&runner->ui, runner->pixels, runner->width,
		runner->height, runner->width, &config) != 0)
		return -1;
	if (sf2000_input_open(&runner->input, "/dev/input/event0") == 0)
		runner->input_open = 1;
	if (runner->framebuffer_phys && hcge_open_context(&runner->ge_storage) == 0) {
		runner->ge = &runner->ge_storage;
		runner->ge_source = hcge_linux_alloc_buffer(runner->ge,
		(unsigned int)(runner->width * runner->height * sizeof(*runner->pixels)),
		&runner->ge_source_phys, &runner->ge_source_handle);
		if (!runner->ge_source) {
			hcge_close_context(runner->ge);
			runner->ge = NULL;
		}
	}
	return 0;
}

static void runner_close(struct js2300_runner *runner)
{
	if (runner->ge) {
		(void)hcge_engine_sync(runner->ge);
		if (runner->ge_source_handle)
			(void)hcge_linux_free_buffer(runner->ge, runner->ge_source_handle);
		hcge_close_context(runner->ge);
	}
	if (runner->input_open)
		sf2000_input_close(&runner->input);
	if (!runner->background)
		sf2000_ui_close(&runner->ui);
	free(runner->pixels);
	if (runner->fb_fd >= 0)
		close(runner->fb_fd);
}

static int runner_run(struct js2300_runner *runner, const char *path)
{
	struct js2300_config config;
	struct js2300_host host;
	struct js2300_runtime *runtime = NULL;
	const char *slash;
	char root[JS2300_MAX_PATH];
	char entry[128];
	size_t root_length;
	int ret;

	slash = strrchr(path, '/');
	if (!slash || slash == path || strlen(slash + 1) >= sizeof(entry))
		return -1;
	root_length = (size_t)(slash - path);
	if (root_length >= sizeof(root))
		return -1;
	memcpy(root, path, root_length);
	root[root_length] = '\0';
	memcpy(entry, slash + 1, strlen(slash + 1) + 1u);
	js2300_config_init(&config);
	config.app_root = root;
	config.entry_script = entry;
	config.heap_bytes = JS2300_HEAP_BYTES;
	runner_configure_host(runner, &host);
	runner_log(runner->background ? "background script start" : "UI script start");
	ret = js2300_runtime_create(&config, &host, &runtime);
	if (ret == 0)
		ret = js2300_runtime_run(runtime);
	js2300_runtime_destroy(runtime);
	return ret;
}

int main(int argc, char **argv)
{
	struct js2300_runner runner;
	const char *path;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s [--background] SCRIPT.js\n", argv[0]);
		return 2;
	}
	memset(&runner, 0, sizeof(runner));
	runner.fb_fd = -1;
	runner.input.fd = -1;
	if (argc == 3) {
		if (strcmp(argv[1], "--background"))
			return 2;
		runner.background = 1;
		path = argv[2];
	} else {
		path = argv[1];
	}
	if (!runner.background && runner_open_ui(&runner) != 0) {
		runner_close(&runner);
		return 1;
	}
	runner_log(path);
	if (access(path, R_OK) != 0)
		return 1;
	int ret = runner_run(&runner, path);
	runner_close(&runner);
	return ret == 0 ? 0 : 1;
}
