#ifndef SF2000_MUFROG_WCHAR_COMPAT_H
#define SF2000_MUFROG_WCHAR_COMPAT_H

#include <stddef.h>

/* Buildroot deliberately omits uClibc wide-character support.  FBAlpha only
 * uses these routines to turn its static ASCII driver names into log/menu
 * strings, so a small ASCII-compatible implementation keeps that feature
 * without pulling a locale or floating-point dependency into every core. */
static inline size_t sf2000_mufrog_wcslen(const wchar_t *string)
{
	const wchar_t *end = string;

	while (*end)
		end++;
	return (size_t)(end - string);
}

static inline size_t sf2000_mufrog_wcstombs(char *destination,
	const wchar_t *source, size_t length)
{
	size_t index;

	if (!destination)
		return sf2000_mufrog_wcslen(source);
	for (index = 0; index < length && source[index]; index++)
		destination[index] = source[index] < 0x80 ? (char)source[index] : '?';
	if (index < length)
		destination[index] = '\0';
	return index;
}

#define wcslen sf2000_mufrog_wcslen
#define wcstombs sf2000_mufrog_wcstombs

#endif
