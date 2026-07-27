// SPDX-License-Identifier: MIT
#ifndef SF2000_PACER_H
#define SF2000_PACER_H

#include <stdint.h>
#include <time.h>

struct sf2000_pacer {
	struct timespec deadline;
	int64_t frame_ns;
	unsigned valid;
	unsigned resets;
	unsigned interval_late_frames;
	unsigned interval_max_late_us;
};

void sf2000_pacer_init(struct sf2000_pacer *pacer, int64_t frame_ns,
	const struct timespec *now);
void sf2000_pacer_invalidate(struct sf2000_pacer *pacer);
int sf2000_pacer_step(struct sf2000_pacer *pacer,
	const struct timespec *now, struct timespec *sleep_deadline);
void sf2000_pacer_reset_interval(struct sf2000_pacer *pacer);

#endif
