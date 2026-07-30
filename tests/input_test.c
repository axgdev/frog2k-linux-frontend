// SPDX-License-Identifier: MIT
#include "sf2000_input.h"
#include "libretro_min.h"

#include <fcntl.h>
#include <linux/input.h>
#include <string.h>
#include <unistd.h>

static int send_key(int fd, unsigned code, int value)
{
	struct input_event event;

	memset(&event, 0, sizeof(event));
	event.type = EV_KEY;
	event.code = code;
	event.value = value;
	return write(fd, &event, sizeof(event)) == (ssize_t)sizeof(event) ? 0 : 1;
}

int main(void)
{
	struct sf2000_input input = { .fd = -1 };
	int pipe_fd[2];
	unsigned actions;

	if (pipe(pipe_fd) < 0 ||
			fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK) < 0)
		return 1;
	input.fd = pipe_fd[0];
	if (send_key(pipe_fd[1], BTN_SELECT, 1) ||
			send_key(pipe_fd[1], BTN_TR, 1))
		return 1;
	actions = sf2000_input_poll(&input);
	if (!(actions & SF2000_INPUT_TOGGLE_UNCAPPED) ||
			input.polls != 1 || input.events != 2 ||
			sf2000_input_state(&input, 0, RETRO_DEVICE_JOYPAD, 0,
				RETRO_DEVICE_ID_JOYPAD_SELECT) ||
			sf2000_input_state(&input, 0, RETRO_DEVICE_JOYPAD, 0,
				RETRO_DEVICE_ID_JOYPAD_R))
		return 1;
	if (sf2000_input_poll(&input) != SF2000_INPUT_NONE)
		return 1;
	if (send_key(pipe_fd[1], BTN_TR, 0) ||
			send_key(pipe_fd[1], BTN_SELECT, 0) ||
			send_key(pipe_fd[1], BTN_START, 1) ||
			send_key(pipe_fd[1], BTN_TL, 1))
		return 1;
	actions = sf2000_input_poll(&input);
	if (!(actions & SF2000_INPUT_EXIT) ||
			input.polls != 3 || input.events != 6 ||
			!sf2000_input_state(&input, 0, RETRO_DEVICE_JOYPAD, 0,
				RETRO_DEVICE_ID_JOYPAD_START))
		return 1;
	input.interval_max_latency_us = 123;
	sf2000_input_reset_interval(&input);
	if (input.interval_max_latency_us)
		return 1;
	sf2000_input_close(&input);
	close(pipe_fd[1]);
	return 0;
}
