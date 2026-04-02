#include <stdio.h>
#include <stdlib.h>
#include "read_bmp.h"
#include "write_bmp.h"

/*Error-checks fwrite calls, printing the given message if the number of arguments written 
is not as expected.*/
void check_fwrite(FILE *bmp, int r, int expected, char *message) {
    if (r != expected) {
        fprintf(stderr, "%s", message);
        fclose(bmp);
        exit(1);
    }
}

void write_bitmap(char *name, int width, int height, struct pixel **pixels) {
    
    // initializing file header for the bitmap
    struct file_info bf = {
        .type = {'B', 'M'},             // indicates file type is bitmap
        .file_size = 3*width*height,    // total # bytes, updated later
        .reserved = 0,                  // not used for our purposes
        .offset = 54                    // pixel array always starts at 54
    };
    
    // initializing bitmap info header
    struct bmp_header bh = {
        .header_size = 40,              // this header is 40 bytes
        .width = width,                 // image width in pixels
        .height = height,               // image height in pixels
        .planes = 1,                    // # color planes (this is always 1)
        .bit_count = 24,                // it's a 24-bit bitmap image
        .compression = 0,               // no compression
        .image_size = 0,                // this field is usually ignored
        .ppm_x = 3780,                  // image resolution x, y in ppm
        .ppm_y = 3780,                  
        .clr_used = 0,                  // # colors in color palette (0 defaults to 2^n)
        .clr_important = 0              // # important colors (0 means all are important)
    };

    // update the total # of bytes with the sizes of the two headers
    bf.file_size += sizeof(bf) + sizeof(bh); 

    // create file object to write to
    FILE *bmp = fopen(name, "wb"); 

    // error-checking fopen
    if (bmp == NULL) {
        perror("fopen");
        exit(1);
    }
    
    // store return value of write calls for error-checking
    int r;

    // write in the file header 
    r = fwrite(&bf, sizeof(bf), 1, bmp);
    check_fwrite(bmp, r, 1, "failed to write file header\n"); 

    // write in the bitmap info header
    r = fwrite(&bh, sizeof(bh), 1, bmp);
    check_fwrite(bmp, r, 1, "failed to write bmp info header\n");

    // write in the pixel array, pixel by pixel
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            float red = pixels[i][j].red;
            float green = pixels[i][j].green;
            float blue = pixels[i][j].blue;

            // write the rgb values in bgr order
            unsigned char color[3] = {blue, green, red};

            r = fwrite(color, sizeof(color), 1, bmp);
            check_fwrite(bmp, r, 1, "failed to write pixel\n");
        }
    }

    // close the finished bitmap file
    fclose(bmp);    

}
