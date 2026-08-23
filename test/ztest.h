#ifndef ZTEST_H
#define ZTEST_H
#include <stdio.h>
#include <string.h>

extern int zt_run_count;
extern int zt_fail_count;

#define ZT_CHECK(c) do { zt_run_count++; if (!(c)) { zt_fail_count++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define ZT_EQ_INT(a,b) do { long _a=(long)(a), _b=(long)(b); zt_run_count++; if (_a!=_b) { \
    zt_fail_count++; printf("FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)

#define ZT_EQ_STR(a,b) do { const char *_a=(a), *_b=(b); zt_run_count++; \
    if (_a==NULL || _b==NULL || strcmp(_a,_b)!=0) { zt_fail_count++; \
    printf("FAIL %s:%d: %s == \"%s\", expected \"%s\"\n", __FILE__, __LINE__, #a, _a?_a:"(null)", _b?_b:"(null)"); } } while (0)

#define ZT_EQ_MEM(a,b,n) do { zt_run_count++; if (memcmp((a),(b),(n))!=0) { zt_fail_count++; \
    printf("FAIL %s:%d: %s differs from %s over %u bytes\n", __FILE__, __LINE__, #a, #b, (unsigned)(n)); } } while (0)

extern int zt_failures(void);
#endif
