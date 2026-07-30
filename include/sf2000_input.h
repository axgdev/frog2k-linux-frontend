// SPDX-License-Identifier: MIT
#ifndef SF2000_INPUT_H
#define SF2000_INPUT_H

#include <stdint.h>

enum sf2000_input_action {
	SF2000_INPUT_NONE = 0,
	SF2000_INPUT_PAUSE = 1u << 0,
};

struct sf2000_input {
	int fd;
	uint32_t keys;
	unsigned pause_chord_latched;
	unsigned polls;
	unsigned events;
	unsigned interval_max_latency_us;
};

int sf2000_input_open(struct sf2000_input *input, const char *path);
void sf2000_input_close(struct sf2000_input *input);
unsigned sf2000_input_poll(struct sf2000_input *input);
int16_t sf2000_input_state(const struct sf2000_input *input, unsigned port,
	unsigned device, unsigned index, unsigned id);
void sf2000_input_reset_interval(struct sf2000_input *input);

#endif
