#ifndef TC_SERVOS_H
#define TC_SERVOS_H

extern int x, y;

void servo_init();
bool move_x(int newpos, bool print_errors = 1, bool smooth = false);
bool move_y(int newpos, bool print_errors = 1, bool smooth = false);
void maybe_update_servos();
void servos_alloc_time(int us);

#endif

