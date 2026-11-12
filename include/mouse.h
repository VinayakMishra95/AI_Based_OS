#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

void mouse_init(int32_t max_w, int32_t max_h);
int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
bool mouse_left_down(void);
bool mouse_left_clicked(void);

#endif
