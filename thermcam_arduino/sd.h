#ifndef TC_SD_H
#define TC_SD_H

#include "common.h"

#if SD_ENABLED == 1
void sd_init();
bool sd_open_new_file(int ymin, int ymax, int xmin, int xmax);
void sd_dump_data(double *temps, int y, int x_start, int x_count);
void sd_remove_file();
void sd_close_file();
#else
static inline void sd_init(){}
static inline bool sd_open_new_file(int ymin, int ymax, int xmin, int xmax){ return true; }
static inline void sd_dump_data(double *temps, int y, int x_start, int x_count){}
static inline void sd_remove_file(){}
static inline void sd_close_file(){}
#endif

#endif

