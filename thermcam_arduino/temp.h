#ifndef TC_TEMP_H
#define TC_TEMP_H

void temp_init();

enum sensor {ambient = 0x6, object = 0x7};
bool read_temp(enum sensor s, double *temp);

#endif

