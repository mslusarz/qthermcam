#ifndef TC_COMMON_H
#define TC_COMMON_H

#define IR_ENABLED 1
#define JOY_ENABLED 1
#define SD_ENABLED 1

#define TC_DEBUG 0

bool use_serial();

void print(const char *s);
void print(char c);
void print(int i);
void print(double f);

void println(const char *s);
void println(int i);
void println(double f);

#include <avr/pgmspace.h>
#define _(s) pgm2ram(PSTR(s))
const char *pgm2ram(PROGMEM char *str);

#endif

