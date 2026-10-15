#ifndef WG_PRINTF_COMPAT_H
#define WG_PRINTF_COMPAT_H
#include <stdio.h>
#if defined(_MSC_VER) && _MSC_VER < 1900
#include <stdarg.h>
/* Older MSVC lacks snprintf and may leave truncated _vsnprintf output open. */
static int WG_CompatSnprintf(char *buffer, size_t size, const char *format, ...)
{
    int result;
    va_list arguments;
    va_start(arguments, format);
    result = _vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    if (size) buffer[size - 1] = '\0';
    return result;
}
#define snprintf WG_CompatSnprintf
#endif
#endif
