#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int showMessageCodes;
extern int verbose;

int error(const char *fmt, ...);
int nerror(int messageNumber, ...);
void message(const char *fmt, ...);
void nmessage(int messageNumber, ...);
void progress(const char *fmt, ...);
void vmessage(const char *fmt, va_list ap, int eol);

#ifdef __cplusplus
}
#endif

#endif
