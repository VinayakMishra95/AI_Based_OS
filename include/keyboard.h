#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

void keyboard_init(void);
bool keyboard_has_char(void);
char keyboard_getc(void);

#endif
