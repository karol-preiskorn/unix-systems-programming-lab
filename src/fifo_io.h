#ifndef FIFO_IO_H
#define FIFO_IO_H

#include <stddef.h>

int fifo_read_messages(const char *path);
int fifo_write_messages(const char *path, size_t message_count);

#endif
