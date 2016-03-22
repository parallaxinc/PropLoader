#ifndef __EXPR_H__
#define __EXPR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <setjmp.h>

/* parse context */
typedef struct {
    int (*findSymbol)(void *cookie, const char *name, int *pValue);
    void *cookie;
    jmp_buf errorTarget;
    char *linePtr;
    int savedTkn;
    int showErrors;
} ParseContext;

/* defined in expr.c */
int ParseNumericExpr(ParseContext *c, const char *token, int *pValue);
int TryParseNumericExpr(ParseContext *c, const char *str, int *pValue);

#ifdef __cplusplus
}
#endif

#endif
