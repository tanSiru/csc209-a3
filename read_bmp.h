#ifndef READ_BMP_H_
#define READ_BMP_H_

#include <stdio.h>

struct pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
};

void read_metadata(FILE *bmp, int *pix_start, int *width, int *height);
struct pixel **read_bmp(FILE *bmp, int pix_start, int width, int height);

#endif /* READ_BMP_H_*/
