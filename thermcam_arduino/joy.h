#ifndef TC_JOY_H
#define TC_JOY_H

#include "common.h"

#if JOY_ENABLED == 1
void joy_init();
bool joystick_button_pressed();
void read_joystick(int &h, int &h_scaled, int &v, int &v_scaled, bool &pressed);
#else
static inline void joy_init() {}
static inline bool joystick_button_pressed() { return false; }
static inline void read_joystick(int &h, int &h_scaled, int &v, int &v_scaled, bool &pressed) {}
#endif

#endif

