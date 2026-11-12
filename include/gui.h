#ifndef GUI_H
#define GUI_H

#include "types.h"

void gui_init(void);
void gui_handle_key(char c);
void gui_handle_serial_byte(char c);
void gui_update_and_draw(void);

#endif
