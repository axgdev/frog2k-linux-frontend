// SPDX-License-Identifier: MIT

#include "libretro_min.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifdef __mips__
#include <sys/cachectl.h>
extern int cacheflush(void *address, int bytes, int cache);
#endif

#define READY_MARKER "/run/sf2000-frontend-ready"

struct host {
	int fb_fd, input_fd;
	uint16_t *fb;
	size_t fb_bytes;
	unsigned fb_width, fb_height, fb_stride;
	uint32_t keys;
	enum retro_pixel_format format;
	double fps;
	const char *system_dir;
	const char *save_dir;
};

static struct host host = { .fb_fd = -1, .input_fd = -1,
	.format = RETRO_PIXEL_FORMAT_0RGB1555, .fps = 60.0 };
static volatile sig_atomic_t stopping;
static int first_frame;

static void log_kmsg(const char *message)
{
	char line[160];
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
	int length;

	if (fd < 0)
		return;
	length = snprintf(line, sizeof(line), "<6>sf2000-frontend: %s", message);
	if (length > 0)
		(void)write(fd, line, (size_t)length);
	close(fd);
}

static void stop_signal(int signal_number)
{
	(void)signal_number;
	stopping = 1;
}

static bool environment(unsigned command, void *data)
{
	switch (command) {
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		host.format = *(enum retro_pixel_format *)data;
		return host.format == RETRO_PIXEL_FORMAT_RGB565 ||
			host.format == RETRO_PIXEL_FORMAT_XRGB8888;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char **)data = host.system_dir;
		return true;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		*(const char **)data = host.save_dir;
		return true;
	case RETRO_ENVIRONMENT_SET_GEOMETRY:
		return true;
	default:
		return false;
	}
}

static uint16_t xrgb8888_to_565(uint32_t pixel)
{
	return (uint16_t)(((pixel >> 8) & 0xf800u) |
		((pixel >> 5) & 0x07e0u) | ((pixel >> 3) & 0x001fu));
}

static void video(const void *data, unsigned width, unsigned height,
	size_t pitch)
{
	unsigned out_w, out_h, left, top, y;

	if (!data || !host.fb || !width || !height)
		return;
	out_w = width > host.fb_width ? host.fb_width : width;
	out_h = height > host.fb_height ? host.fb_height : height;
	left = (host.fb_width - out_w) / 2u;
	top = (host.fb_height - out_h) / 2u;
	for (y = 0; y < out_h; y++) {
		uint16_t *dst = host.fb + (top + y) * host.fb_stride + left;
		unsigned src_y = y * height / out_h;
		unsigned x;

		if (host.format == RETRO_PIXEL_FORMAT_RGB565 && out_w == width) {
			memcpy(dst, (const uint8_t *)data + src_y * pitch,
				out_w * sizeof(*dst));
			continue;
		}
		for (x = 0; x < out_w; x++) {
			unsigned src_x = x * width / out_w;
			if (host.format == RETRO_PIXEL_FORMAT_RGB565)
				dst[x] = *(const uint16_t *)((const uint8_t *)data +
					src_y * pitch + src_x * 2u);
			else
				dst[x] = xrgb8888_to_565(*(const uint32_t *)(
					(const uint8_t *)data + src_y * pitch + src_x * 4u));
		}
	}
#ifdef __mips__
	(void)cacheflush(host.fb, (int)host.fb_bytes, BCACHE);
#endif
	if (!first_frame) {
		int fd = open(READY_MARKER,
			O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

		if (fd >= 0)
			close(fd);
		first_frame = 1;
	}
}

static size_t audio_batch(const int16_t *samples, size_t frames)
{
	(void)samples;
	/* ALSA DMA integration is the next platform service; never stall a core. */
	return frames;
}

static void audio_sample(int16_t left, int16_t right)
{
	int16_t pair[2] = { left, right };
	(void)audio_batch(pair, 1);
}

static unsigned key_bit(unsigned code)
{
	switch (code) {
	case BTN_SOUTH: return 1u << RETRO_DEVICE_ID_JOYPAD_B;
	case BTN_WEST: return 1u << RETRO_DEVICE_ID_JOYPAD_Y;
	case BTN_SELECT: return 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
	case BTN_START: return 1u << RETRO_DEVICE_ID_JOYPAD_START;
	case BTN_DPAD_UP: return 1u << RETRO_DEVICE_ID_JOYPAD_UP;
	case BTN_DPAD_DOWN: return 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
	case BTN_DPAD_LEFT: return 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
	case BTN_DPAD_RIGHT: return 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
	case BTN_EAST: return 1u << RETRO_DEVICE_ID_JOYPAD_A;
	case BTN_NORTH: return 1u << RETRO_DEVICE_ID_JOYPAD_X;
	case BTN_TL: return 1u << RETRO_DEVICE_ID_JOYPAD_L;
	case BTN_TR: return 1u << RETRO_DEVICE_ID_JOYPAD_R;
	default: return 0;
	}
}

static void input_poll(void)
{
	struct input_event event;

	while (host.input_fd >= 0 && read(host.input_fd, &event, sizeof(event)) ==
			(ssize_t)sizeof(event)) {
		unsigned bit;
		if (event.type != EV_KEY || !(bit = key_bit(event.code)))
			continue;
		if (event.value)
			host.keys |= bit;
		else
			host.keys &= ~bit;
	}
	if ((host.keys & (1u << RETRO_DEVICE_ID_JOYPAD_SELECT)) &&
			(host.keys & (1u << RETRO_DEVICE_ID_JOYPAD_Y)))
		stopping = 1;
}

static int16_t input_state(unsigned port, unsigned device, unsigned index,
	unsigned id)
{
	(void)index;
	if (port || device != RETRO_DEVICE_JOYPAD || id >= 32)
		return 0;
	return (host.keys & (1u << id)) != 0;
}

static int open_platform(void)
{
	struct fb_fix_screeninfo fix;
	struct fb_var_screeninfo var;

	host.fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if (host.fb_fd < 0 || ioctl(host.fb_fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
			ioctl(host.fb_fd, FBIOGET_VSCREENINFO, &var) < 0)
		return -1;
	if (var.bits_per_pixel != 16) {
		errno = ENOTSUP;
		return -1;
	}
	host.fb_width = var.xres;
	host.fb_height = var.yres;
	host.fb_stride = fix.line_length / sizeof(uint16_t);
	host.fb_bytes = fix.smem_len;
	host.fb = mmap(NULL, host.fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED,
		host.fb_fd, 0);
	if (host.fb == MAP_FAILED) {
		host.fb = NULL;
		return -1;
	}
	host.input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	return host.input_fd < 0 ? -1 : 0;
}

static void close_platform(void)
{
	if (host.fb)
		munmap(host.fb, host.fb_bytes);
	if (host.fb_fd >= 0)
		close(host.fb_fd);
	if (host.input_fd >= 0)
		close(host.input_fd);
}

int main(int argc, char **argv)
{
	struct retro_system_info info;
	struct retro_system_av_info av;
	struct retro_game_info game = { 0 };
	struct timespec deadline;
	long frame_ns;

	if (argc != 2) {
		fprintf(stderr, "usage: %s ROM\n", argv[0]);
		return 2;
	}
	(void)unlink(READY_MARKER);
	if (open_platform() < 0) {
		log_kmsg("platform open failed\n");
		perror("sf2000-frontend: platform");
		close_platform();
		return 1;
	}
	host.system_dir = "/mnt/sd/bios";
	host.save_dir = "/mnt/sd/saves";
	retro_set_environment(environment);
	retro_set_video_refresh(video);
	retro_set_audio_sample(audio_sample);
	retro_set_audio_sample_batch(audio_batch);
	retro_set_input_poll(input_poll);
	retro_set_input_state(input_state);
	if (retro_api_version() != RETRO_API_VERSION) {
		fprintf(stderr, "sf2000-frontend: incompatible libretro API\n");
		close_platform();
		return 1;
	}
	memset(&info, 0, sizeof(info));
	retro_get_system_info(&info);
	retro_init();
	game.path = argv[1];
	if (!retro_load_game(&game)) {
		log_kmsg("core rejected game\n");
		fprintf(stderr, "sf2000-frontend: core rejected %s\n", argv[1]);
		retro_deinit();
		close_platform();
		return 1;
	}
	retro_get_system_av_info(&av);
	host.fps = av.timing.fps > 1.0 ? av.timing.fps : 60.0;
	frame_ns = (long)(1000000000.0 / host.fps);
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	log_kmsg("frontend running SELECT+Y exits\n");
	signal(SIGINT, stop_signal);
	signal(SIGTERM, stop_signal);
	while (!stopping) {
		retro_run();
		deadline.tv_nsec += frame_ns;
		while (deadline.tv_nsec >= 1000000000L) {
			deadline.tv_nsec -= 1000000000L;
			deadline.tv_sec++;
		}
		(void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
	}
	retro_unload_game();
	retro_deinit();
	close_platform();
	log_kmsg("returned cleanly\n");
	return 0;
}
