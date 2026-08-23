#include <stdio.h>
#include "cfg_io.h"
int cfg_io_read_file(const char *path, char *buf, unsigned cap)
{
    FILE *f = fopen(path, "rb"); size_t n;
    if (!f) return -1;
    n = fread(buf, 1, cap, f); fclose(f);
    return (int)n;
}
