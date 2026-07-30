/* Minimal libretro API subset used by the static SF2000 host. */
#ifndef SF2000_LIBRETRO_MIN_H
#define SF2000_LIBRETRO_MIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RETRO_API_VERSION 1
#define RETRO_ENVIRONMENT_GET_CAN_DUPE 3
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
#define RETRO_ENVIRONMENT_GET_VARIABLE 15
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31
#define RETRO_ENVIRONMENT_SET_GEOMETRY 37
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
#define RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE (81 | 0x10000)
#define RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER (40 | 0x10000)
#define RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION 52
#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS 53
#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL 54
#define RETRO_DEVICE_JOYPAD 1
#define RETRO_DEVICE_ID_JOYPAD_B 0
#define RETRO_DEVICE_ID_JOYPAD_Y 1
#define RETRO_DEVICE_ID_JOYPAD_SELECT 2
#define RETRO_DEVICE_ID_JOYPAD_START 3
#define RETRO_DEVICE_ID_JOYPAD_UP 4
#define RETRO_DEVICE_ID_JOYPAD_DOWN 5
#define RETRO_DEVICE_ID_JOYPAD_LEFT 6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT 7
#define RETRO_DEVICE_ID_JOYPAD_A 8
#define RETRO_DEVICE_ID_JOYPAD_X 9
#define RETRO_DEVICE_ID_JOYPAD_L 10
#define RETRO_DEVICE_ID_JOYPAD_R 11

enum retro_pixel_format {
	RETRO_PIXEL_FORMAT_0RGB1555 = 0,
	RETRO_PIXEL_FORMAT_XRGB8888 = 1,
	RETRO_PIXEL_FORMAT_RGB565 = 2
};

struct retro_game_geometry {
	unsigned base_width, base_height, max_width, max_height;
	float aspect_ratio;
};
struct retro_system_timing { double fps, sample_rate; };
struct retro_system_av_info {
	struct retro_game_geometry geometry;
	struct retro_system_timing timing;
};
struct retro_system_info {
	const char *library_name, *library_version, *valid_extensions;
	bool need_fullpath, block_extract;
};
struct retro_game_info {
	const char *path;
	const void *data;
	size_t size;
	const char *meta;
};
struct retro_variable {
	const char *key;
	const char *value;
};
enum retro_log_level {
	RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR,
	RETRO_LOG_DUMMY = INT32_MAX
};
typedef void (*retro_log_printf_t)(enum retro_log_level, const char *, ...);
struct retro_log_callback { retro_log_printf_t log; };

struct retro_framebuffer {
	void *data;
	unsigned width, height;
	size_t pitch;
	enum retro_pixel_format format;
	unsigned access_flags;
	unsigned memory_flags;
};
#define RETRO_MEMORY_ACCESS_WRITE (1u << 0)
#define RETRO_MEMORY_ACCESS_READ (1u << 1)
#define RETRO_MEMORY_TYPE_CACHED (1u << 0)

typedef bool (*retro_environment_t)(unsigned, void *);
typedef void (*retro_video_refresh_t)(const void *, unsigned, unsigned, size_t);
typedef void (*retro_audio_sample_t)(int16_t, int16_t);
typedef size_t (*retro_audio_sample_batch_t)(const int16_t *, size_t);
typedef void (*retro_input_poll_t)(void);
typedef int16_t (*retro_input_state_t)(unsigned, unsigned, unsigned, unsigned);

void retro_set_environment(retro_environment_t);
void retro_set_video_refresh(retro_video_refresh_t);
void retro_set_audio_sample(retro_audio_sample_t);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
void retro_set_input_poll(retro_input_poll_t);
void retro_set_input_state(retro_input_state_t);
void retro_init(void);
void retro_deinit(void);
unsigned retro_api_version(void);
void retro_get_system_info(struct retro_system_info *);
void retro_get_system_av_info(struct retro_system_av_info *);
bool retro_load_game(const struct retro_game_info *);
void retro_unload_game(void);
void retro_run(void);

#endif
