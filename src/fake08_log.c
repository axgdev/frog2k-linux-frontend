/* fake-08's z8lua port expects these firmware logging hooks.  Errors are
 * already reported through the libretro log callback; keep this compatibility
 * hook silent so an exceptional path cannot stall the frame loop. */

#include <stdarg.h>

void xlog(const char *format, ...)
{
	(void)format;
}

void xlog_clear(void)
{
}
