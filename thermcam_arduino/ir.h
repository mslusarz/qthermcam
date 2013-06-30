#ifndef TC_IR_H
#define TC_IR_H

enum ir_button { NONE, LEFT, UP, RIGHT, DOWN, LEFT_TOP, RIGHT_BOTTOM, STOP};

void ir_init();
bool infrared_stop_button_pressed();
enum ir_button infrared_any_button_pressed();

#endif

