// SPDX-License-Identifier: MIT
#include "libretro_min.h"
#include <string.h>

static retro_video_refresh_t video;
static unsigned frame;
void retro_set_environment(retro_environment_t cb) { enum retro_pixel_format f = RETRO_PIXEL_FORMAT_RGB565; cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &f); }
void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) { frame = 0; }
void retro_deinit(void) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *i) { memset(i, 0, sizeof(*i)); i->library_name = "dummy"; }
void retro_get_system_av_info(struct retro_system_av_info *i) { memset(i, 0, sizeof(*i)); i->geometry.base_width = 320; i->geometry.base_height = 240; i->timing.fps = 60; }
bool retro_load_game(const struct retro_game_info *g) { return g != 0; }
void retro_unload_game(void) { }
void retro_run(void) { static uint16_t pixels[320 * 240]; unsigned i; for (i = 0; i < 320 * 240; i++) pixels[i] = (uint16_t)(i + frame); frame++; video(pixels, 320, 240, 640); }
