// SPDX-License-Identifier: MIT
#include "sf2000_pacer.h"

#include <assert.h>

static struct timespec milliseconds(long value)
{
	struct timespec time = {
		.tv_sec = value / 1000,
		.tv_nsec = value % 1000 * 1000000,
	};
	return time;
}

int main(void)
{
	struct sf2000_pacer pacer;
	struct timespec now = milliseconds(0);
	struct timespec deadline;

	sf2000_pacer_init(&pacer, 10000000, &now);
	now = milliseconds(5);
	assert(sf2000_pacer_step(&pacer, &now, &deadline));
	assert(deadline.tv_nsec == 10000000);

	now = milliseconds(21);
	assert(!sf2000_pacer_step(&pacer, &now, &deadline));
	assert(pacer.interval_late_frames == 1);
	assert(pacer.resets == 0);

	now = milliseconds(25);
	assert(sf2000_pacer_step(&pacer, &now, &deadline));
	assert(deadline.tv_nsec == 30000000);

	now = milliseconds(51);
	assert(!sf2000_pacer_step(&pacer, &now, &deadline));
	assert(pacer.resets == 1);
	assert(pacer.deadline.tv_nsec == 51000000);

	sf2000_pacer_invalidate(&pacer);
	now = milliseconds(100);
	assert(sf2000_pacer_step(&pacer, &now, &deadline));
	assert(deadline.tv_nsec == 110000000);
	sf2000_pacer_reset_interval(&pacer);
	assert(pacer.interval_late_frames == 0);
	assert(pacer.interval_max_late_us == 0);
	assert(pacer.resets == 1);
	return 0;
}
