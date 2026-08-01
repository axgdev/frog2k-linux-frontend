// SPDX-License-Identifier: MIT

#include "libretro_min.h"

#ifndef CORE_PREFIX
#error "CORE_PREFIX must name a prefixed libretro archive"
#endif

#define CORE_SYMBOL_JOIN(prefix, name) prefix##_##name
#define CORE_SYMBOL(prefix, name) CORE_SYMBOL_JOIN(prefix, name)
#define CORE(name) CORE_SYMBOL(CORE_PREFIX, name)

extern void CORE(retro_set_environment)(retro_environment_t);
extern void CORE(retro_set_video_refresh)(retro_video_refresh_t);
extern void CORE(retro_set_audio_sample)(retro_audio_sample_t);
extern void CORE(retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
extern void CORE(retro_set_input_poll)(retro_input_poll_t);
extern void CORE(retro_set_input_state)(retro_input_state_t);
extern void CORE(retro_init)(void);
extern void CORE(retro_deinit)(void);
extern unsigned CORE(retro_api_version)(void);
extern void CORE(retro_get_system_info)(struct retro_system_info *);
extern void CORE(retro_get_system_av_info)(struct retro_system_av_info *);
extern bool CORE(retro_load_game)(const struct retro_game_info *);
extern void CORE(retro_unload_game)(void);
extern void CORE(retro_run)(void);
extern void *CORE(retro_get_memory_data)(unsigned);
extern size_t CORE(retro_get_memory_size)(unsigned);
extern size_t CORE(retro_serialize_size)(void);
extern bool CORE(retro_serialize)(void *, size_t);
extern bool CORE(retro_unserialize)(const void *, size_t);

void retro_set_environment(retro_environment_t callback)
{
	CORE(retro_set_environment)(callback);
}

void retro_set_video_refresh(retro_video_refresh_t callback)
{
	CORE(retro_set_video_refresh)(callback);
}

void retro_set_audio_sample(retro_audio_sample_t callback)
{
	CORE(retro_set_audio_sample)(callback);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback)
{
	CORE(retro_set_audio_sample_batch)(callback);
}

void retro_set_input_poll(retro_input_poll_t callback)
{
	CORE(retro_set_input_poll)(callback);
}

void retro_set_input_state(retro_input_state_t callback)
{
	CORE(retro_set_input_state)(callback);
}

void retro_init(void)
{
	CORE(retro_init)();
}

void retro_deinit(void)
{
	CORE(retro_deinit)();
}

unsigned retro_api_version(void)
{
	return CORE(retro_api_version)();
}

void retro_get_system_info(struct retro_system_info *info)
{
	CORE(retro_get_system_info)(info);
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	CORE(retro_get_system_av_info)(info);
}

bool retro_load_game(const struct retro_game_info *game)
{
	return CORE(retro_load_game)(game);
}

void retro_unload_game(void)
{
	CORE(retro_unload_game)();
}

void retro_run(void)
{
	CORE(retro_run)();
}

void *retro_get_memory_data(unsigned id)
{
	return CORE(retro_get_memory_data)(id);
}

size_t retro_get_memory_size(unsigned id)
{
	return CORE(retro_get_memory_size)(id);
}

size_t retro_serialize_size(void)
{
	return CORE(retro_serialize_size)();
}

bool retro_serialize(void *data, size_t size)
{
	return CORE(retro_serialize)(data, size);
}

bool retro_unserialize(const void *data, size_t size)
{
	return CORE(retro_unserialize)(data, size);
}
