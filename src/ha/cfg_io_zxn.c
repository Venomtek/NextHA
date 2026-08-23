#include <arch/zxn/esxdos.h>
#include <errno.h>
#include "cfg_io.h"
int cfg_io_read_file(const char *path, char *buf, unsigned cap)
{
    unsigned char h; unsigned n; int rc;
    errno = 0;
    h = esx_f_open(path, ESX_MODE_READ | ESX_MODE_OPEN_EXIST);
    if (errno) return -1;
    n = esx_f_read(h, buf, cap);
    rc = errno ? -1 : (int)n;      /* decide before closing: a close error must not */
    esx_f_close(h);                /* turn a read that already succeeded into a failure */
    return rc;
}
