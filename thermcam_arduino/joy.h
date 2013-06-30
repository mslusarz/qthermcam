#ifndef TC_JOY_H
#define TC_JOY_H

void joy_init();
bool joystick_button_pressed();
void read_joystick(int &h, int &h_scaled, int &v, int &v_scaled, bool &pressed);

#endif

