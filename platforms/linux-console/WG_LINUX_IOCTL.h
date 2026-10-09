#ifndef WG_LINUX_IOCTL_H
#define WG_LINUX_IOCTL_H

#include <sys/ioctl.h>

/* Linux ioctl requests carry 32 bits. glibc takes unsigned long, whereas
 * musl takes int. Explicit conversion preserves the request's bit pattern
 * without an implicit unsigned-constant overflow diagnostic on musl. Keep
 * the third argument's original type (some requests take an integer).
 */
#if defined(__GLIBC__)
#define WG_IOCTL(fd, request, argument) \
    ioctl((fd), (unsigned long)(request), (argument))
#else
#define WG_IOCTL(fd, request, argument) \
    ioctl((fd), (int)(request), (argument))
#endif

#endif
