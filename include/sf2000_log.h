// SPDX-License-Identifier: MIT
#ifndef SF2000_LOG_H
#define SF2000_LOG_H

/* Ask sf2000-logd to persist its current RAM journal and wait for its ack. */
int sf2000_log_flush(const char *reason);

/*
 * Enter the latency-sensitive session only after core initialization and ROM
 * loading have succeeded.  Keeping startup outside the RAM-only journal makes
 * a loader/core-init failure diagnosable even when retained RAM is unavailable.
 */
int sf2000_performance_begin(void);
void sf2000_performance_end(void);

#endif
