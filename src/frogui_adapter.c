// SPDX-License-Identifier: MIT
/* Adapt UniFrog's symbol-prefixed FrogUI libretro core to the standard ABI. */
#include "libretro_min.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

extern void frogui_retro_set_environment(retro_environment_t);
extern void frogui_retro_set_video_refresh(retro_video_refresh_t);
extern void frogui_retro_set_audio_sample(retro_audio_sample_t);
extern void frogui_retro_set_audio_sample_batch(retro_audio_sample_batch_t);
extern void frogui_retro_set_input_poll(retro_input_poll_t);
extern void frogui_retro_set_input_state(retro_input_state_t);
extern void frogui_retro_init(void);
extern void frogui_retro_deinit(void);
extern unsigned frogui_retro_api_version(void);
extern void frogui_retro_get_system_info(struct retro_system_info *);
extern void frogui_retro_get_system_av_info(struct retro_system_av_info *);
extern bool frogui_retro_load_game(const struct retro_game_info *);
extern void frogui_retro_unload_game(void);
extern void frogui_retro_run(void);

/* The SF2000 FrogUI archive is compiled for the native 320x240 panel. */
#define FROGUI_PIXELS (320u * 240u)
static uint16_t frogui_framebuffer[FROGUI_PIXELS];
extern void *__real_calloc(size_t count, size_t size);
extern void __real_free(void *pointer);

void *__wrap_calloc(size_t count, size_t size)
{
	if (count == FROGUI_PIXELS && size == sizeof(uint16_t))
		return frogui_framebuffer;
	return __real_calloc(count, size);
}

void __wrap_free(void *pointer)
{
	if (pointer != frogui_framebuffer)
		__real_free(pointer);
}

void retro_set_environment(retro_environment_t cb) { frogui_retro_set_environment(cb); }
void retro_set_video_refresh(retro_video_refresh_t cb) { frogui_retro_set_video_refresh(cb); }
void retro_set_audio_sample(retro_audio_sample_t cb) { frogui_retro_set_audio_sample(cb); }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { frogui_retro_set_audio_sample_batch(cb); }
void retro_set_input_poll(retro_input_poll_t cb) { frogui_retro_set_input_poll(cb); }
void retro_set_input_state(retro_input_state_t cb) { frogui_retro_set_input_state(cb); }
void retro_init(void) { frogui_retro_init(); }
void retro_deinit(void) { frogui_retro_deinit(); }
unsigned retro_api_version(void) { return frogui_retro_api_version(); }
void retro_get_system_info(struct retro_system_info *info) { frogui_retro_get_system_info(info); }
void retro_get_system_av_info(struct retro_system_av_info *info) { frogui_retro_get_system_av_info(info); }
bool retro_load_game(const struct retro_game_info *game) { return frogui_retro_load_game(game); }
void retro_unload_game(void) { frogui_retro_unload_game(); }
void retro_run(void) { frogui_retro_run(); }

/* Configuration is optional for the first Linux port; FrogUI has defaults. */
int unifrog_config_read(const char *path, void *callback, void *userdata,
	unsigned *errors)
{
	(void)path; (void)callback; (void)userdata;
	if (errors) *errors = 0;
	return -1;
}

int unifrog_config_replace_section(const char *path, const char *section,
	int (*writer)(FILE *, void *), void *userdata)
{
	(void)path; (void)section; (void)writer; (void)userdata;
	return -1;
}

void __assert_func(const char *file, int line, const char *function,
	const char *expression)
{
	fprintf(stderr, "assertion failed: %s:%d %s: %s\n", file, line,
		function ? function : "?", expression);
	abort();
}
