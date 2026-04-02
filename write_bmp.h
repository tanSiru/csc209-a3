#ifndef WRITE_BMP_H_
#define WRITE_BMP_H_

#include "read_bmp.h"

#pragma pack(push, 1) // force compiler to not add extra padding so sizes/offsets are exact
struct file_info {
    unsigned char type[2];
    int file_size;
    int reserved;
    unsigned int offset;
};

struct bmp_header {
    unsigned int header_size;
    unsigned int width;
    unsigned int height;
    short int planes;
    short int bit_count;
    unsigned int compression;
    unsigned int image_size;
    int ppm_x;
    int ppm_y;
    unsigned int clr_used;
    unsigned int clr_important;
};
#pragma pack(pop)

void write_bitmap(char *name, int width, int height, struct pixel **pixels);

#endif /* WRITE_BMP_H_*/
