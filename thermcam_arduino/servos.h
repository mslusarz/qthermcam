#ifndef TC_SERVOS_H
#define TC_SERVOS_H

extern int x, y;

void servo_init();
bool move_x(int newpos, bool print_errors = 1);
bool move_y(int newpos, bool print_errors = 1);

#endif

