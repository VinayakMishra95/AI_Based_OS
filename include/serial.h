#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

void serial_init(void);
void serial_putc(char c);
void serial_write(const char* str);
bool serial_has_data(void);
int  serial_getc_nonblock(void);

#endif
