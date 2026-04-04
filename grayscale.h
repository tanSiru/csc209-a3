#ifndef GRAYSCALE_H
#define GRAYSCALE_H

#include "read_bmp.h"
#include "encoding.h"

struct pixel **apply_grayscale(struct pixel **pixel_array, int height, int width);

#endif