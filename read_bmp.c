#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "read_bmp.h"

#define PIX_START 10
#define WIDTH 18
#define HEIGHT 22

/*Error-checks fseek calls, closing the program and printing an error if r != 0.*/
void check_fseek(FILE *bmp, int r) {
    if (r != 0) {
        perror("fseek");
        fclose(bmp);
        exit(1);
    }
}

/*Error-checks fread calls, printing an error message if the number of arguments read 
is not as expected.*/
void check_fread(FILE *bmp, int r, int expected) {
    if (r != expected) {
        fprintf(stderr, "fread failed");
        fclose(bmp);
        exit(1);
    }
}

/*Given an open file pointer to a valid .bmp file, stores the pixel array starting point, 
width, and height of the bmp into pix_start, width, and height, respectively. */
void read_metadata(FILE *bmp, int *pix_start, int *width, int *height) {
    // store return values of sys/library calls for cleaner error-checking
    int fseek_r; 
    int fread_r;

    // read in pixel array offset
    fseek_r = fseek(bmp, PIX_START, SEEK_SET);
    check_fseek(bmp, fseek_r);                       // error-checking fseek
    fread_r = fread(pix_start, sizeof(int), 1, bmp); // store pixel array offset in pix_start
    check_fread(bmp, fread_r, 1);                    // error-check fread to make sure 1 argument was read

    // read in image width
    fseek_r = fseek(bmp, WIDTH, SEEK_SET);
    check_fseek(bmp, fseek_r);
    fread_r = fread(width, sizeof(int), 1, bmp);
    check_fread(bmp, fread_r, 1);

    // read in image height
    fseek_r = fseek(bmp, HEIGHT, SEEK_SET);
    check_fseek(bmp, fseek_r);
    fread_r = fread(height, sizeof(int), 1, bmp);
    check_fread(bmp, fread_r, 1);
}

struct pixel **read_bmp(FILE *bmp, int pix_start, int width, int height) {
    struct pixel **pixarray = malloc(height * sizeof(struct pixel *));
    
    // set bmp to point to beginning of pixarray
    check_fseek(bmp, fseek(bmp, pix_start, SEEK_SET)); // also does error-checking

    // read in the bmp, one row at a time
    for (int i = 0; i < height; i++) {
    	pixarray[i] = malloc(width * sizeof(struct pixel));           // allocate space for each row 
        int r = fread(pixarray[i], sizeof(struct pixel), width, bmp); // read in entire row
        check_fread(bmp, r, width);                                   // error-checking
	
    }
    return pixarray;

}



