// SPDX-License-Identifier: MIT
#include "sf2000_pacer.h"

#include <limits.h>
#include <string.h>

static void add_ns(struct timespec *time, int64_t nanoseconds)
{
	time->tv_sec += nanoseconds / 1000000000;
	time->tv_nsec += nanoseconds % 1000000000;
	if (time->tv_nsec >= 1000000000L) {
		time->tv_nsec -= 1000000000L;
		time->tv_sec++;
	}
}

static int after(const struct timespec *left, const struct timespec *right)
{
	return left->tv_sec > right->tv_sec ||
		(left->tv_sec == right->tv_sec &&
		 left->tv_nsec > right->tv_nsec);
}

static uint64_t difference_ns(const struct timespec *later,
	const struct timespec *earlier)
{
	uint64_t nanoseconds =
		(uint64_t)(later->tv_sec - earlier->tv_sec) * 1000000000u;

	if (later->tv_nsec >= earlier->tv_nsec)
		nanoseconds += (uint64_t)(later->tv_nsec - earlier->tv_nsec);
	else
		nanoseconds -= (uint64_t)(earlier->tv_nsec - later->tv_nsec);
	return nanoseconds;
}

void sf2000_pacer_init(struct sf2000_pacer *pacer, int64_t frame_ns,
	const struct timespec *now)
{
	memset(pacer, 0, sizeof(*pacer));
	pacer->frame_ns = frame_ns > 0 ? frame_ns : 16666667;
	pacer->deadline = *now;
	pacer->valid = 1;
}

void sf2000_pacer_invalidate(struct sf2000_pacer *pacer)
{
	pacer->valid = 0;
}

int sf2000_pacer_step(struct sf2000_pacer *pacer,
	const struct timespec *now, struct timespec *sleep_deadline)
{
	uint64_t late_ns;
	uint64_t late_us;

	if (!pacer->valid) {
		pacer->deadline = *now;
		pacer->valid = 1;
	}
	add_ns(&pacer->deadline, pacer->frame_ns);
	if (!after(now, &pacer->deadline)) {
		*sleep_deadline = pacer->deadline;
		return 1;
	}

	late_ns = difference_ns(now, &pacer->deadline);
	late_us = late_ns / 1000u;
	pacer->interval_late_frames++;
	if (late_us > pacer->interval_max_late_us)
		pacer->interval_max_late_us = late_us > UINT_MAX ?
			UINT_MAX : (unsigned)late_us;

	/*
	 * Preserve the absolute timeline for a sub-frame miss. The next cheap
	 * frame can recover that time naturally. Rebase only when at least one
	 * complete frame has already been lost; replaying work beyond that point
	 * would race through stale emulation and overfill audio.
	 */
	if (late_ns >= (uint64_t)pacer->frame_ns) {
		pacer->deadline = *now;
		pacer->resets++;
	}
	return 0;
}

void sf2000_pacer_reset_interval(struct sf2000_pacer *pacer)
{
	pacer->interval_late_frames = 0;
	pacer->interval_max_late_us = 0;
}
