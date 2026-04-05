#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "read_bmp.h"
#include "encoding.h"

void check_malloc(void *returned) {
    if (returned == NULL) {
        perror("malloc");
        exit(1);
    }
}

void check_write(int returned) {
    if (returned == -1) {
        perror("write");
        exit(1);
    }
}

void check_read(int returned) {
    if (returned == -1) {
        perror("read");
        exit(1);
    }
}

void check_close(int returned) {
    if (returned == -1) {
        perror("close");
        exit(1);
    }
}

struct pixel *flip_row(struct pixel *row, int width) {
    struct pixel *result = malloc(width * sizeof(struct pixel));
    for (int i = 0; i < width; i++) {
        result[i] = row[width-i-1];
    }
    return result;
}

struct pixel **apply_flip(struct pixel **pixels, int height, int width) {

    // pipes for each child to receive and send data
    int fd[height][2][2];

    // child pids for wait calls later
    pid_t child_pids[height];

    for (int i = 0; i < height; i++) {
        // create pipe for parent->child
        if (pipe(fd[i][0]) == -1) { 
            perror("pipe");
            exit(1);
        }

        // create pipe for child->parent
        if (pipe(fd[i][1]) == -1) { 
            perror("pipe"); 
            exit(1);
        }

        // create a child for each row of the image
        int r = fork();

        // parent process
        if (r > 0) {                
            check_close(close(fd[i][0][0]));    // close read end of parent->child pipe
            check_close(close(fd[i][1][1]));    // close write end of child->parent pipe

            child_pids[i] = r;                  // store pid of child

            // initialize Message containing row data for child
            Message *init_row = malloc(sizeof(Message) + sizeof(struct pixel) * width);
            check_malloc(init_row);

            init_row->row = i;
            memcpy(init_row->pixel_row, pixels[i], width * sizeof(struct pixel));   

            // write row of pixels to child for processing
            check_write(write(fd[i][0][1], init_row, sizeof(Message) + width * sizeof(struct pixel)));
            
            check_close(close(fd[i][0][1]));   // close write pipe after writing the row
        }

        else if (r == 0) { // child
            close(fd[i][0][1]);  // close write end of parent->child pipe
            close(fd[i][1][0]);  // close read end of child->parent pipe

            Message *row = malloc(sizeof(Message) + width * sizeof(struct pixel));
            check_malloc(row);

            check_read(read(fd[i][0][0], row, sizeof(Message) + width * sizeof(struct pixel)));

            int row_id = row->row;
            struct pixel *data = row->pixel_row;

            // apply flip transformation to row
            struct pixel *flipped = flip_row(data, width);

            Message *result = malloc(sizeof(Message) + sizeof(struct pixel) * width);
            check_malloc(result);

            result->row = row_id;
            memcpy(result->pixel_row, flipped, width * sizeof(struct pixel));
            
            // write the encoded result to the results pipe
            check_write(write(fd[i][1][1], result, sizeof(Message) + width * sizeof(struct pixel)));
            
            // free memory from earlier mallocs
            free(row);
            free(flipped);
            
            // close all pipes before exiting
            close(fd[i][0][0]);
            close(fd[i][1][1]);
            exit(0);
        }

        else { // if fork returns -1
            perror("fork");
            exit(1);
        }
    }

    struct pixel **new_pixels = malloc(height * sizeof(struct pixel *));
    
    for (int i = 0; i < height; i++) {
        int status;

        Message *received = malloc(sizeof(Message) + sizeof(struct pixel) * width);

        if (waitpid(child_pids[i], &status, 0) == -1) {
            perror("wait");
            exit(1);
        }
        
        if(WIFEXITED(status)) {
            check_read(read(fd[i][1][0], received, sizeof(Message) + width * sizeof(struct pixel)));

            new_pixels[i] = malloc(width * sizeof(struct pixel));
            memcpy(new_pixels[i], received->pixel_row, width * sizeof(struct pixel)); 

            check_close(close(fd[i][1][0]));
        }
        // else: handle error by making new child to handle row
    }

    return new_pixels;
}
