/* system.h - an elf and spin binary loader for the Parallax Propeller microcontroller

Copyright (c) 2011 David Michael Betz

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#if defined(WIN32)
#define PATH_SEP    ';'
#define DIR_SEP     '\\'
#define DIR_SEP_STR "\\"
#else
#define PATH_SEP    ':'
#define DIR_SEP     '/'
#define DIR_SEP_STR "/"
#endif

int xbAddPath(const char *path);
int xbAddFilePath(const char *name);
int xbAddEnvironmentPath(const char *name);
int xbAddProgramPath(char *argv[]);
FILE *xbOpenFileInPath(const char *name, const char *mode);

#ifdef __cplusplus
}
#endif

#endif
