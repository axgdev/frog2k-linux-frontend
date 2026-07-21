// SPDX-License-Identifier: MIT
#include <stddef.h>
#include <stdint.h>

const char *utf8skip(const char *text, size_t characters)
{
	while (*text && characters--) {
		text++;
		while ((*text & 0xc0) == 0x80)
			text++;
	}
	return text;
}

uint32_t utf8_walk(const char **cursor)
{
	const unsigned char *text = (const unsigned char *)*cursor;
	uint32_t value;
	unsigned continuation;

	if (*text < 0x80) {
		*cursor += *text != 0;
		return *text;
	}
	if ((*text & 0xe0) == 0xc0) { value = *text++ & 0x1f; continuation = 1; }
	else if ((*text & 0xf0) == 0xe0) { value = *text++ & 0x0f; continuation = 2; }
	else if ((*text & 0xf8) == 0xf0) { value = *text++ & 0x07; continuation = 3; }
	else { (*cursor)++; return 0xfffd; }
	while (continuation--) {
		if ((*text & 0xc0) != 0x80) { (*cursor)++; return 0xfffd; }
		value = (value << 6) | (*text++ & 0x3f);
	}
	*cursor = (const char *)text;
	return value;
}
