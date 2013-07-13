#ifndef TC_IR_H
#define TC_IR_H

#include "common.h"

enum ir_button { NONE, LEFT, UP, RIGHT, DOWN, LEFT_TOP, RIGHT_BOTTOM, STOP};

#if IR_ENABLED == 1
void ir_init();
bool infrared_stop_button_pressed();
enum ir_button infrared_any_button_pressed();
#else
static inline void ir_init() {}
static inline bool infrared_stop_button_pressed() { return false; }
static inline enum ir_button infrared_any_button_pressed() { return NONE; }
#endif

#endif

