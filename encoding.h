#include "read_bmp.h"

typedef struct {
    int row;
    struct pixel pixel_row[];
} Message;