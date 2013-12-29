#ifndef TC_COMMON_H
#define TC_COMMON_H

#define IR_ENABLED 1
#define JOY_ENABLED 1
#define SD_ENABLED 1

#define TC_DEBUG 0

bool use_serial();

#define DECL_PRINT(type) \
void print(type v); \
void println(type v);

DECL_PRINT(const char *);
DECL_PRINT(char);
DECL_PRINT(int);
DECL_PRINT(double);
DECL_PRINT(unsigned long);

#include <avr/pgmspace.h>
#define _(s) pgm2ram(PSTR(s))
const char *pgm2ram(PROGMEM const char *str);

int get_vreg_voltage100();

#endif

