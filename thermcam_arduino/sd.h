#ifndef TC_SD_H
#define TC_SD_H

//#define SD_ENABLED

#ifdef SD_ENABLED
void sd_init();
void sd_open_new_file(int ymin, int ymax, int xmin, int xmax);
void sd_dump_data(double *temps, int y, int x_start, int x_count);
void sd_remove_file();
void sd_close_file();
#else
static inline void sd_init(){}
static inline void sd_open_new_file(int ymin, int ymax, int xmin, int xmax){}
static inline void sd_dump_data(double *temps, int y, int x_start, int x_count){}
static inline void sd_remove_file(){}
static inline void sd_close_file(){}
#endif

#endif

