// SPDX-License-Identifier: MIT
#include "sf2000_input.h"
#include "libretro_min.h"

#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <unistd.h>

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

int sf2000_input_open(struct sf2000_input *input, const char *path)
{
	memset(input, 0, sizeof(*input));
	input->fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	return input->fd < 0 ? -1 : 0;
}

void sf2000_input_close(struct sf2000_input *input)
{
	if (input->fd >= 0)
		close(input->fd);
	input->fd = -1;
}

unsigned sf2000_input_poll(struct sf2000_input *input)
{
	const unsigned chord =
		(1u << RETRO_DEVICE_ID_JOYPAD_SELECT) |
		(1u << RETRO_DEVICE_ID_JOYPAD_R);
	struct input_event event;
	unsigned actions = SF2000_INPUT_NONE;

	while (input->fd >= 0 &&
			read(input->fd, &event, sizeof(event)) ==
				(ssize_t)sizeof(event)) {
		unsigned bit;

		if (event.type != EV_KEY || !(bit = key_bit(event.code)))
			continue;
		if (event.value)
			input->keys |= bit;
		else
			input->keys &= ~bit;
		if (event.value && (input->keys & chord) == chord &&
				!input->uncapped_chord_latched) {
			input->uncapped_chord_latched = 1;
			actions |= SF2000_INPUT_TOGGLE_UNCAPPED;
		} else if (!event.value && (bit & chord)) {
			input->uncapped_chord_latched = 0;
		}
	}
	if ((input->keys & (1u << RETRO_DEVICE_ID_JOYPAD_START)) &&
			(input->keys & (1u << RETRO_DEVICE_ID_JOYPAD_L)))
		actions |= SF2000_INPUT_EXIT;
	if ((input->keys & chord) != chord &&
			!(input->keys & (1u << RETRO_DEVICE_ID_JOYPAD_SELECT)) &&
			!(input->keys & (1u << RETRO_DEVICE_ID_JOYPAD_R)))
		input->uncapped_chord_latched = 0;
	return actions;
}

int16_t sf2000_input_state(const struct sf2000_input *input, unsigned port,
	unsigned device, unsigned index, unsigned id)
{
	(void)index;
	if (port || device != RETRO_DEVICE_JOYPAD || id >= 32)
		return 0;
	if (input->uncapped_chord_latched &&
			(id == RETRO_DEVICE_ID_JOYPAD_SELECT ||
			 id == RETRO_DEVICE_ID_JOYPAD_R))
		return 0;
	return (input->keys & (1u << id)) != 0;
}
