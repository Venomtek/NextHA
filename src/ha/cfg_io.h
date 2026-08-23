#ifndef NEXTHA_CFG_IO_H
#define NEXTHA_CFG_IO_H

int cfg_io_read_file(const char *path, char *buf, unsigned cap);  /* bytes read, or -1 if cannot open; truncates at cap */

#endif
