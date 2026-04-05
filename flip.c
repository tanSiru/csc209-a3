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

    // allocate space for newly modified pixel array to reconstruct image
    struct pixel **new_pixels = malloc(height * sizeof(struct pixel *));

    int worker_count = 0;
    for (int i = 0; i < height; i++) {

        if (pipe(fd[i][0]) == -1) {             // pipe parent->child
            perror("pipe");
            exit(1);
        }

        if (pipe(fd[i][1]) == -1) {             // pipe child->parent
            perror("pipe"); 
            exit(1);
        }

        int r = fork();

        // parent process
        if (r > 0) {     
            worker_count++;                     // update count of active children
            
            check_close(close(fd[i][0][0]));    // close read end of parent->child pipe
            check_close(close(fd[i][1][1]));    // close write end of child->parent pipe

            child_pids[i] = r;                  // store pid of child

            // initialize Message containing row data for child
            Message *init_row = malloc(sizeof(Message) + sizeof(struct pixel) * width);
            check_malloc(init_row);
            init_row->row = i;

            // copy pixel data into Message
            memcpy(init_row->pixel_row, pixels[i], width * sizeof(struct pixel));   

            // write row of pixels to child for processing
            check_write(write(fd[i][0][1], init_row, sizeof(Message) + width * sizeof(struct pixel))); // TODO: replace w write loop
            // TODO: implement advanced error checking for write fail
            
            // close write pipe after writing the row
            check_close(close(fd[i][0][1]));
            
            // if number of workers reaches 10, call wait to avoid overflow
            if (worker_count > 9) {
                for (int j = worker_count; j > 0; j--) {
                int status;

                Message *received = malloc(sizeof(Message) + sizeof(struct pixel) * width);

                if (waitpid(child_pids[i-(j-1)], &status, 0) == -1) {
                    perror("wait");
                    exit(1);
                }
                
                if(WIFEXITED(status)) {
                    check_read(read(fd[i-(j-1)][1][0], received, sizeof(Message) + width * sizeof(struct pixel)));

                    new_pixels[i-(j-1)] = malloc(width * sizeof(struct pixel));
                    memcpy(new_pixels[i-(j-1)], received->pixel_row, width * sizeof(struct pixel)); 

                    check_close(close(fd[i-(j-1)][1][0]));
                }
                // else: handle error by making new child to handle row
                }
                worker_count = 0;
            }
        }

        // child process
        else if (r == 0) { 
            check_close(close(fd[i][0][1]));  // close write end of parent->child pipe
            check_close(close(fd[i][1][0]));  // close read end of child->parent pipe

            // allocate space for Message received from parent
            Message *row = malloc(sizeof(Message) + width * sizeof(struct pixel));
            check_malloc(row);

            // read Message from parent into prev allocated space
            check_read(read(fd[i][0][0], row, sizeof(Message) + width * sizeof(struct pixel))); // TODO: replace w read loop
            int row_id = row->row;
            struct pixel *data = row->pixel_row;

            // apply flip transformation to row
            struct pixel *flipped = flip_row(data, width);

            // initialize a return message with the new row for the parent
            Message *result = malloc(sizeof(Message) + sizeof(struct pixel) * width);
            check_malloc(result);
            result->row = row_id;
            memcpy(result->pixel_row, flipped, width * sizeof(struct pixel));   // using memcpy to copy bytes directly into Message
            
            // write the encoded result to the results pipe
            check_write(write(fd[i][1][1], result, sizeof(Message) + width * sizeof(struct pixel))); // TODO: replace w write loop
            
            // free memory from earlier mallocs
            free(row);
            free(flipped);
            
            // close all pipes before exiting
            close(fd[i][0][0]);
            close(fd[i][1][1]);
            exit(0);
        }

        // if fork returns -1
        else { 
            perror("fork");
            exit(1);
        }
    }

    // call wait on any remaining workers
    for (int j = worker_count; j > 0; j--) {
        int status;

        Message *received = malloc(sizeof(Message) + sizeof(struct pixel) * width);

        if (waitpid(child_pids[height-j], &status, 0) == -1) {
            perror("wait");
                exit(1);
            }
                
            if(WIFEXITED(status)) {
                check_read(read(fd[height-j][1][0], received, sizeof(Message) + width * sizeof(struct pixel)));

                new_pixels[height-j] = malloc(width * sizeof(struct pixel));
                memcpy(new_pixels[height-j], received->pixel_row, width * sizeof(struct pixel)); 

                check_close(close(fd[height-j][1][0]));
            }
                // else: handle error by making new child to handle row
        }

    return new_pixels;
}
