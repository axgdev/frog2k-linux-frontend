// SPDX-License-Identifier: MIT
#ifndef SF2000_LOG_H
#define SF2000_LOG_H

/* Ask sf2000-logd to persist its current RAM journal and wait for its ack. */
int sf2000_log_flush(const char *reason);

#endif
