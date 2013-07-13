#ifndef TC_COMMON_H
#define TC_COMMON_H

extern char buf[100];
#define TC_DEBUG 0

bool use_serial();

void print(const char *s);
void print(double f);
void println(const char *s);
void println(double f);

#endif

