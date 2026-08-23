#include <arch/zxn.h>
#include "bank.h"
#include "../ha_errors.h"

static unsigned char saved6, saved7, mapped;

int bank_map(unsigned char bank16)
{
    if (bank16 > 111) return ERR_ARG_BANK;           /* 2MB Next: 16K banks 0..111 */
    if (!mapped) { saved6 = ZXN_READ_MMU6(); saved7 = ZXN_READ_MMU7(); mapped = 1; }
    ZXN_WRITE_MMU6((unsigned char)(bank16 * 2));
    ZXN_WRITE_MMU7((unsigned char)(bank16 * 2 + 1));
    return 0;
}

void bank_unmap(void)
{
    if (mapped) { ZXN_WRITE_MMU6(saved6); ZXN_WRITE_MMU7(saved7); mapped = 0; }
}
