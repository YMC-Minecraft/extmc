#ifndef _COMMON_H
#define _COMMON_H

#include <libintl.h>
#define _(X) gettext(X)

#ifdef DISABLE_DEBUG
#define DEBUG(fmt)
#define DEBUGF(fmt, ...)
#else
#define DEBUG(fmt, ...) do { fprintf(stdout, fmt); } while (0)
#define DEBUGF(fmt, ...) do { fprintf(stdout, fmt, __VA_ARGS__); } while (0)
#endif

#endif // _COMMON_H
