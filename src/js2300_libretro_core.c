// SPDX-License-Identifier: MIT
//
// JS2300 libretro core for the static SF2000 Linux frontend.
//
// The JS2300 runtime is synchronous: js2300_runtime_run() loads and evaluates
// the whole script and invokes the host callbacks (video, input, fs, ...)
// from inside that evaluation.  The target uClibc has no pthread support, so
// the script is executed directly from retro_run() and every video.present()
// the script performs is delivered immediately through the libretro video
// refresh callback.  The frontend already filters transient frames published
// during retro_load_game(), so the runtime is only started on the first
// retro_run() call, which is also where the ownership handoff happens.
//
// Note: js2300_runtime_run() is fully synchronous and the frontend wraps
// retro_run() in an alarm-based watchdog, so scripts with long-running loops
// must stay within that budget; one-shot UI/background scripts are fine.

#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libretro_min.h"
#include "unifrog/abi.h"
#include <js2300/js2300.h>

#define JS2300_CORE_WIDTH 320u
#define JS2300_CORE_HEIGHT 240u
#define JS2300_CORE_HEAP_BYTES (8u * 1024u * 1024u)

/* JS2300 input bit layout (see js2300.h / UniFrog binding). */
#define JS2300_INPUT_UP (1u << 0)
#define JS2300_INPUT_DOWN (1u << 1)
#define JS2300_INPUT_LEFT (1u << 2)
#define JS2300_INPUT_RIGHT (1u << 3)
#define JS2300_INPUT_A (1u << 4)
#define JS2300_INPUT_B (1u << 5)
#define JS2300_INPUT_X (1u << 6)
#define JS2300_INPUT_Y (1u << 7)
#define JS2300_INPUT_START (1u << 8)
#define JS2300_INPUT_SELECT (1u << 9)

struct js2300_core_state {
	uint16_t pixels[JS2300_CORE_WIDTH * JS2300_CORE_HEIGHT];
	struct js2300_runtime *runtime;
	int started;
	int finished;
};

static struct js2300_core_state g_core;
static retro_environment_t g_env;
static retro_video_refresh_t g_video;
static retro_audio_sample_t g_audio;
static retro_audio_sample_batch_t g_audio_batch;
static retro_input_poll_t g_input_poll;
static retro_input_state_t g_input_state;

static void core_host_log(void *opaque, const char *message)
{
	(void)opaque;
	(void)message;
}

static int core_flush_log(void *opaque)
{
	(void)opaque;
	return 0;
}

static uint32_t core_millis(void *opaque)
{
	struct timespec ts;
	(void)opaque;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (uint32_t)((uint64_t)ts.tv_sec * 1000u +
		(uint64_t)ts.tv_nsec / 1000000u);
}

static void core_sleep_ms(void *opaque, uint32_t ms)
{
	(void)opaque;
	(void)poll(NULL, 0, (int)ms);
}

static void core_video_clear(void *opaque, uint16_t color)
{
	struct js2300_core_state *core = opaque;
	unsigned i;

	if (!core)
		return;
	for (i = 0; i < JS2300_CORE_WIDTH * JS2300_CORE_HEIGHT; i++)
		core->pixels[i] = color;
}

static void core_video_rects(void *opaque, const struct js2300_rect *rects,
	size_t count)
{
	struct js2300_core_state *core = opaque;
	size_t i;

	if (!core || !rects)
		return;
	for (i = 0; i < count; i++) {
		int x = rects[i].x;
		int y = rects[i].y;
		int w = rects[i].w;
		int h = rects[i].h;
		int cx;
		int cy;
		uint16_t color = rects[i].color;

		if (x < 0) {
			w += x;
			x = 0;
		}
		if (y < 0) {
			h += y;
			y = 0;
		}
		if (w > (int)JS2300_CORE_WIDTH - x)
			w = (int)JS2300_CORE_WIDTH - x;
		if (h > (int)JS2300_CORE_HEIGHT - y)
			h = (int)JS2300_CORE_HEIGHT - y;
		if (w <= 0 || h <= 0)
			continue;
		for (cy = y; cy < y + h; cy++)
			for (cx = x; cx < x + w; cx++)
				core->pixels[cy * JS2300_CORE_WIDTH + cx] = color;
	}
}

static void core_video_text(void *opaque, int x, int y, const char *text,
	uint16_t color)
{
	(void)opaque;
	(void)x;
	(void)y;
	(void)text;
	(void)color;
	/* Text rendering is not available in this minimal adapter. */
}

static int core_video_image(void *opaque, const char *path, int x, int y,
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

static void core_video_present(void *opaque)
{
	struct js2300_core_state *core = opaque;

	if (!core || !g_video)
		return;
	g_video(core->pixels, JS2300_CORE_WIDTH, JS2300_CORE_HEIGHT,
		JS2300_CORE_WIDTH * sizeof(uint16_t));
}

static uint32_t core_input_poll(void *opaque)
{
	struct js2300_core_state *core = opaque;
	uint32_t keys = 0;

	if (g_input_poll)
		g_input_poll();
	if (!core || !g_input_state)
		return 0;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
		keys |= JS2300_INPUT_UP;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
		keys |= JS2300_INPUT_DOWN;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
		keys |= JS2300_INPUT_LEFT;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
		keys |= JS2300_INPUT_RIGHT;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
		keys |= JS2300_INPUT_B;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
		keys |= JS2300_INPUT_A;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
		keys |= JS2300_INPUT_Y;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
		keys |= JS2300_INPUT_X;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
		keys |= JS2300_INPUT_START;
	if (g_input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
		keys |= JS2300_INPUT_SELECT;
	return keys;
}

static void core_battery(void *opaque, struct js2300_battery_status *status)
{
	(void)opaque;
	if (!status)
		return;
	status->percent = -1;
	status->charging = 0;
	status->low = 0;
}

static int core_fs_list(void *opaque, const char *path,
	struct js2300_fs_entry *entries, size_t max_entries)
{
	(void)opaque;
	(void)path;
	(void)entries;
	(void)max_entries;
	return -1;
}

static int core_fs_read_text(void *opaque, const char *path,
	char *out, size_t out_size)
{
	FILE *file;
	size_t got;
	int error;
	(void)opaque;

	if (!path || !out || !out_size || (file = fopen(path, "rb")) == NULL)
		return -1;
	got = fread(out, 1, out_size - 1u, file);
	error = ferror(file);
	fclose(file);
	if (error)
		return -1;
	out[got] = '\0';
	return (int)got;
}

static int core_file_size(void *opaque, const char *path, size_t *out_size)
{
	FILE *file;
	long end;

	(void)opaque;
	if (!path || !out_size || (file = fopen(path, "rb")) == NULL)
		return -1;
	if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
		fclose(file) != 0)
		return -1;
	*out_size = (size_t)end;
	return 0;
}

static int core_file_read(void *opaque, const char *path, void *out,
	size_t capacity, size_t *out_size)
{
	FILE *file;
	size_t size;
	size_t got;

	(void)opaque;
	if (!out_size || core_file_size(NULL, path, &size) != 0 ||
		size > capacity || (size != 0 && !out) ||
		(file = fopen(path, "rb")) == NULL)
		return -1;
	got = fread(out, 1, size, file);
	if (got != size || ferror(file) || fclose(file) != 0)
		return -1;
	*out_size = got;
	return 0;
}

static int core_fs_write_text(void *opaque, const char *path,
	const char *text, size_t size)
{
	FILE *file;
	int write_ok;
	int close_ok;
	(void)opaque;

	if (!path || !text || (file = fopen(path, "wb")) == NULL)
		return -1;
	write_ok = (!size || fwrite(text, 1, size, file) == size) &&
		fflush(file) == 0;
	close_ok = fclose(file) == 0;
	return write_ok && close_ok ? 0 : -1;
}

static int core_action(void *opaque, const char *id)
{
	(void)opaque;
	(void)id;
	return -1;
}

static void core_exit(void *opaque, const char *reason)
{
	(void)opaque;
	(void)reason;
}

static int core_backlight(void *opaque, int level, int *out_level)
{
	(void)opaque;
	(void)level;
	if (out_level)
		*out_level = -1;
	return -1;
}

static int core_av_output(void *opaque, int mode, int *out_mode)
{
	(void)opaque;
	(void)mode;
	if (out_mode)
		*out_mode = -1;
	return -1;
}

static int core_font_load(void *opaque, const char *path)
{
	(void)opaque;
	(void)path;
	return -1;
}

static void core_configure_host(struct js2300_host *host, void *opaque)
{
	memset(host, 0, sizeof(*host));
	host->size = sizeof(*host);
	host->opaque = opaque;
	host->log = core_host_log;
	host->flush_log = core_flush_log;
	host->millis = core_millis;
	host->sleep_ms = core_sleep_ms;
	host->video_clear = core_video_clear;
	host->video_rects = core_video_rects;
	host->video_text = core_video_text;
	host->video_image = core_video_image;
	host->video_present = core_video_present;
	host->input_poll = core_input_poll;
	host->battery = core_battery;
	host->fs_list = core_fs_list;
	host->action = core_action;
	host->exit = core_exit;
	host->backlight = core_backlight;
	host->av_output = core_av_output;
	host->fs_read_text = core_fs_read_text;
	host->fs_write_text = core_fs_write_text;
	host->font_load = core_font_load;
	host->file_size = core_file_size;
	host->file_read = core_file_read;
}

void retro_set_environment(retro_environment_t cb)
{
	g_env = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	g_video = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	g_audio = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	g_audio_batch = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	g_input_poll = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	g_input_state = cb;
}

void retro_init(void)
{
	unsigned format = RETRO_PIXEL_FORMAT_RGB565;

	memset(&g_core, 0, sizeof(g_core));
	if (g_env)
		(void)g_env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
}

void retro_deinit(void)
{
	if (g_core.runtime) {
		js2300_runtime_destroy(g_core.runtime);
		g_core.runtime = NULL;
	}
	g_core.started = 0;
	g_core.finished = 0;
}

unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = "JS2300";
	info->library_version = "0.7.0";
	info->valid_extensions = "js|mjs|ch8|chip8|zip";
	info->need_fullpath = true;
	info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = JS2300_CORE_WIDTH;
	info->geometry.base_height = JS2300_CORE_HEIGHT;
	info->geometry.max_width = JS2300_CORE_WIDTH;
	info->geometry.max_height = JS2300_CORE_HEIGHT;
	info->geometry.aspect_ratio = 4.0f / 3.0f;
	info->timing.fps = 60.0;
	info->timing.sample_rate = 32000.0;
}

bool retro_load_game(const struct retro_game_info *game)
{
	struct js2300_core_state *core = &g_core;
	struct js2300_config config;
	struct js2300_host host;
	const char *slash;
	const char *path;
	char root[512];
	char entry[128];
	size_t root_length;

	if (core->runtime)
		return false;
	if (!game || !game->path)
		return false;
	path = game->path;
	slash = strrchr(path, '/');
	if (!slash || slash == path || strlen(slash + 1) >= sizeof(entry))
		return false;
	root_length = (size_t)(slash - path);
	if (root_length >= sizeof(root))
		return false;
	memcpy(root, path, root_length);
	root[root_length] = '\0';
	memcpy(entry, slash + 1, strlen(slash + 1) + 1u);

	js2300_config_init(&config);
	config.app_root = root;
	config.entry_script = entry;
	config.heap_bytes = JS2300_CORE_HEAP_BYTES;

	core_configure_host(&host, core);
	if (js2300_runtime_create(&config, &host, &core->runtime) != 0)
		return false;
	return true;
}

void retro_unload_game(void)
{
	if (g_core.runtime) {
		js2300_runtime_destroy(g_core.runtime);
		g_core.runtime = NULL;
	}
	g_core.started = 0;
	g_core.finished = 0;
}

void retro_run(void)
{
	struct js2300_core_state *core = &g_core;

	if (!g_video || !core->runtime)
		return;
	if (core->started) {
		/* Script completed: keep presenting its last frame. */
		g_video(core->pixels, JS2300_CORE_WIDTH, JS2300_CORE_HEIGHT,
			JS2300_CORE_WIDTH * sizeof(uint16_t));
		return;
	}
	core->started = 1;
	(void)js2300_runtime_run(core->runtime);
	core->finished = 1;
}

void *retro_get_memory_data(unsigned id)
{
	(void)id;
	return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	(void)id;
	return 0;
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void *data, size_t size)
{
	(void)data;
	(void)size;
	return false;
}

bool retro_unserialize(const void *data, size_t size)
{
	(void)data;
	(void)size;
	return false;
}
