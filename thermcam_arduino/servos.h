#ifndef TC_SERVOS_H
#define TC_SERVOS_H

extern int x, y;

void servo_init();
void move_x(int newpos, bool print_errors = 1);
void move_y(int newpos, bool print_errors = 1);

#endif

