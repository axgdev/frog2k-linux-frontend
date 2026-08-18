#ifndef SF2000_MUFROG_WCHAR_COMPAT_H
#define SF2000_MUFROG_WCHAR_COMPAT_H

#include <stddef.h>

/* The Buildroot toolchain builds uClibc without wide-character support, while
 * the frog-toolchain uClibc-ng builds it with __UCLIBC_HAS_WCHAR__ and
 * declares wcslen()/wcstombs() itself.  Some cores (FBAlpha, fake08) are
 * patched to call the sf2000_mufrog_* helpers by name, so those always exist;
 * only the wcslen/wcstombs macro remaps are conditional, otherwise they
 * collide with the libc (and libstdc++ <cstdlib>) declarations.  features.h
 * is the sanctioned way to load bits/uClibc_config.h (direct inclusion is
 * rejected with #error). */
#include <features.h>

/* ASCII-compatible implementations: the cores only use these routines to turn
 * static ASCII strings into log/menu text, so they avoid pulling a locale or
 * floating-point dependency into every core. */
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

#if !defined(__UCLIBC_HAS_WCHAR__)
#define wcslen sf2000_mufrog_wcslen
#define wcstombs sf2000_mufrog_wcstombs
#endif

#endif
