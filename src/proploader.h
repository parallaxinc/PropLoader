#ifndef __PROPLOADER_H__
#define __PROPLOADER_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int verbose;

int error(const char *fmt, ...);
void message(const char *fmt, ...);
void progress(const char *fmt, ...);
void vmessage(const char *fmt, va_list ap, int eol);

#ifdef __cplusplus
}
#endif

#endif
