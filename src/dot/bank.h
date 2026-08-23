#ifndef NEXTHA_DOT_BANK_H
#define NEXTHA_DOT_BANK_H

/* Invariant: the dot's code and data must all stay below 0xC000. bank_map() hijacks MMU6
   and MMU7 - the whole 0xC000-0xFFFF window - to page a user bank in, so anything the dot
   placed up there would be overlaid by the bank while it is mapped. build.cmd fails the
   build if the linked image ever reaches that window. */
#define BANK_BASE ((unsigned char *)0xC000)
#define BANK_CAP  16382u                       /* payload capacity after the 2-byte length */

int  bank_map(unsigned char bank16);           /* 0 ok / ERR_ARG_BANK (bank > 111); maps pages 2b,2b+1 at 0xC000/0xE000 */
void bank_unmap(void);                         /* restores the MMU6/7 pages seen at first map; safe to call twice */

#endif
