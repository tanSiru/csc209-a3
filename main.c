#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "read_bmp.h"
#include "grayscale.h"
#include "write_bmp.h"
#include "flip.h"

int get_filter_choice(){
    int num;
    int check = 1;
    while(check){
        printf("Type 1 for grayscale filter, 2 for horizontal flip filter, or 3 for both filters: ");
        scanf("%d", &num);

        if(num != 1 && num != 2 && num != 3){
            printf("Please enter a valid input(1,2, or 3)");
        }else{
            check = 0;
        }
    }

    return num;
}

char *get_file_name(int for_input){
    char *file_name = malloc(sizeof(char)*512);
    int check = 1;
    while(check){
        if (for_input == 1) {
            printf("Input file name(make sure to include .bmp at the end): ");
        }
        else {
            printf("Output file name(make sure to include .bmp at the end): ");
        }
        scanf("%s", file_name);

        if(strstr(file_name, ".bmp")){
            check = 0;
        }else{
            printf("Please ensure you included .bmp at the end.");
        }
    }

    return file_name;
}



int main(){
    int filter_choice = get_filter_choice();
    char *input_filename = get_file_name(1);
    char *output_filename = get_file_name(0);

    FILE *image = fopen(input_filename, "rb");
    if(image==NULL){
        printf("Invalid bitmap file name.  Please retry running the program with a proper path.");
        exit(1);
    }
    
    // Read in bitmap file metadata
    int pixel_array_offset, width, height;
    read_metadata(image, &pixel_array_offset, &width, &height);

    // Read in pixels of bitmap then apply filter
    struct pixel **pixels = read_bmp(image, pixel_array_offset, width, height);

    if(filter_choice == 1){
        struct pixel **gray_pixels = apply_grayscale(pixels, height, width);

        // write to bitmap
        // use a different name than original so that a new bitmap will be created by the system
        FILE *new_image= fopen("grayscale.bmp", "wb");
        if (new_image == NULL) {
            perror("fopen");
            exit(1);
        }

        // reset file pointer
        rewind(image);

        // read in metadata of original bitmap then write it into the new bitmap
        unsigned char header[pixel_array_offset];
        fread(header, 1, pixel_array_offset, image);
        fwrite(header, 1, pixel_array_offset, new_image);

        // calculate the padding needed to make sure to make sure that the all bitmap row have size that is a multiple of 4
        int padding = (4 - (width * 3) % 4) % 4;
        unsigned char pad[3] = {0,0,0};

        for(int i = 0; i < height; i++){
            // first write the pixels then add padding;
            fwrite(gray_pixels[i], sizeof(struct pixel), width, new_image);
            fwrite(pad, 1, padding, new_image);
        }


        // free(file_name);
        fclose(image);
        fclose(new_image);
    }
    else if(filter_choice == 2){
        struct pixel **flipped = apply_flip(pixels, height, width);

        write_bitmap(output_filename, width, height, flipped);
    }
    else if(filter_choice == 3){
        struct pixel **gray_pixels = apply_grayscale(pixels, height, width);
        FILE *new_image= fopen("grayscale.bmp", "wb");

        // reset file pointer
        rewind(image);

        // read in metadata of original bitmap then write it into the new bitmap
        unsigned char header[pixel_array_offset];
        fread(header, 1, pixel_array_offset, image);
        fwrite(header, 1, pixel_array_offset, new_image);

        // calculate the padding needed to make sure to make sure that the all bitmap row have size that is a multiple of 4
        int padding = (4 - (width * 3) % 4) % 4;
        unsigned char pad[3] = {0,0,0};

        for(int i = 0; i < height; i++){
            // first write the pixels then add padding;
            fwrite(gray_pixels[i], sizeof(struct pixel), width, new_image);
            fwrite(pad, 1, padding, new_image);
        }
        

        // free(file_name);
        fclose(image);
        fclose(new_image);

        printf("horizontal flip");

        // ADD HORIZONTAL FLIP FILTER CODE

    }

    
}