#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "read_bmp.h"
#include "encoding.h"

// error-checking helper functions:
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

/* 
Given an input row of pixels, returns a row of pixels flipped horizontally.
*/
struct pixel *flip_row(struct pixel *row, int width) {
    struct pixel *result = malloc(width * sizeof(struct pixel));
    for (int i = 0; i < width; i++) {
        result[i] = row[width-i-1];
    }
    return result;
}

/*
Given an input pixel array, returns a pixel array where all the pixels have been flipped horizontally.
*/
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
            check_write(write(fd[i][0][1], init_row, sizeof(Message) + width * sizeof(struct pixel)));
            
            // close write pipe after writing the row
            check_close(close(fd[i][0][1]));
            free(init_row);
            
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
                else {  // child crashed -> make new child and redo row
                    
                    // set up pipes again for replacement child
                    if (pipe(fd[i-j+1][0]) == -1) {
                        perror("pipe");
                        exit(1);
                    }

                    if (pipe(fd[i-j+1][1]) == -1) {
                        perror("pipe");
                        exit(1);
                    }
                    
                    int new_worker = fork();

                    if (new_worker > 0) {
                        // store new child pid
                        child_pids[i-j+1] = new_worker;

                        // close appropriate pipe ends
                        check_close(close(fd[i-j+1][0][0]));
                        check_close(close(fd[i-j+1][1][1]));
                        
                        // set up Message with failed row data for replacement child
                        Message *replace_row = malloc(sizeof(Message) + width * sizeof(struct pixel));
                        check_malloc(replace_row);
                        replace_row->row = i-j+1;
                        memcpy(replace_row->pixel_row, pixels[i-j+1], width * sizeof(struct pixel));

                        // write Message to replacement child
                        if (write(fd[i-j+1][0][1], replace_row, sizeof(Message) + width * sizeof(struct pixel))== -1) {
                            // if write to replacement worker fails, print error and end program
                            fprintf(stderr, "reassign row to child failed");  
                            exit(1);
                        } 

                        // close write pipe
                        check_close(close(fd[i-j+1][0][1]));

                        // read processed row from child
                        check_read(read(fd[i-j+1][1][0], replace_row, sizeof(Message) + width * sizeof(struct pixel)));

                        // place new row into new pixel array
                        new_pixels[i-j+1] = malloc(width * sizeof(struct pixel));
                        memcpy(new_pixels[i-j+1], replace_row->pixel_row, width * sizeof(struct pixel));
                        
                        int new_status;
                        if (waitpid(child_pids[i-j+1], &new_status, 0) == -1) {
                            perror("wait");
                            exit(1);
                        }

                        check_close(close(fd[i-j+1][1][0]));
                        free(replace_row);
                    }
                    else if (new_worker == 0) {
                        // close appropriate pipe ends
                        check_close(close(fd[i-j+1][0][1]));
                        check_close(close(fd[i-j+1][1][0]));

                        Message *new_row = malloc(sizeof(Message) + width * sizeof(struct pixel));

                        // read row data from parent
                        check_read(read(fd[i-j+1][0][0], new_row, sizeof(Message) + width * sizeof(struct pixel)));

                        // apply flip
                        struct pixel *flipped_row = flip_row(new_row->pixel_row, width);
                        memcpy(new_row->pixel_row, flipped_row, width * sizeof(struct pixel));

                        // close read pipe
                        check_close(close(fd[i-j+1][0][0]));

                        // write to parent
                        if (write(fd[i-j+1][1][1], new_row, sizeof(Message) + width * sizeof(struct pixel)) == -1) {
                            fprintf(stderr, "child write failed again");
                            exit(1);
                        }

                        // free Message, close write pipe
                        free(new_row);
                        check_close(close(fd[i-j+1][1][1]));
                    }
                    else { // fork failed
                        perror("fork");
                        exit(1);
                    }
                 }
                }
                worker_count = 0; // parent resets count of active workers after collecting all of them
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
            check_read(read(fd[i][0][0], row, sizeof(Message) + width * sizeof(struct pixel))); 
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
            check_write(write(fd[i][1][1], result, sizeof(Message) + width * sizeof(struct pixel)));
            
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
            else {  // child crashed -> make new child and redo row
                // set up pipes again for replacement child
                if (pipe(fd[height-j][0]) == -1) {
                    perror("pipe");
                    exit(1);
                }
                if (pipe(fd[height-j][1]) == -1) {
                    perror("pipe");
                    exit(1);
                }
                
                int new_worker = fork();

                if (new_worker > 0) {
                    // store new child pid
                    child_pids[height-j] = new_worker;

                    // close appropriate pipe ends
                    check_close(close(fd[height-j][0][0]));
                    check_close(close(fd[height-j][1][1]));
                    
                    // set up Message with failed row data for replacement child
                    Message *replace_row = malloc(sizeof(Message) + width * sizeof(struct pixel));
                    check_malloc(replace_row);
                    replace_row->row = height-j;
                    memcpy(replace_row->pixel_row, pixels[height-j], width * sizeof(struct pixel));

                    // write Message to replacement child
                    if (write(fd[height-j][0][1], replace_row, sizeof(Message) + width * sizeof(struct pixel))== -1) {
                            // if write to replacement worker fails, print error and end program
                            fprintf(stderr, "reassign row to child failed");  
                            exit(1);
                        } 

                    // close write pipe
                    check_close(close(fd[height-j][0][1]));

                    // read processed row from child
                    check_read(read(fd[height-j][1][0], replace_row, sizeof(Message) + width * sizeof(struct pixel)));

                    // place new row into new pixel array
                    new_pixels[height-j] = malloc(width * sizeof(struct pixel));
                    memcpy(new_pixels[height-j], replace_row->pixel_row, width * sizeof(struct pixel));
                    
                    int new_status;
                    if (waitpid(child_pids[height-j], &new_status, 0) == -1) {
                        perror("wait");
                        exit(1);
                    }

                    check_close(close(fd[height-j][1][0]));
                    free(replace_row);
                }
                else if (new_worker == 0) {
                    // close appropriate pipe ends
                    check_close(close(fd[height-j][0][1]));
                    check_close(close(fd[height-j][1][0]));

                    Message *new_row = malloc(sizeof(Message) + width * sizeof(struct pixel));

                    // read row data from parent
                    check_read(read(fd[height-j][0][0], new_row, sizeof(Message) + width * sizeof(struct pixel)));

                    // apply flip
                    struct pixel *flipped_row = flip_row(new_row->pixel_row, width);
                    memcpy(new_row->pixel_row, flipped_row, width * sizeof(struct pixel));

                    // close read pipe
                    check_close(close(fd[height-j][0][0]));

                    // write to parent
                    if (write(fd[height-j][1][1], new_row, sizeof(Message) + width * sizeof(struct pixel)) == -1) {
                        fprintf(stderr, "child write failed again");
                        exit(1);
                    }

                    // free Message, close write pipe
                    free(new_row);
                    check_close(close(fd[height-j][1][1]));
                }
                else { // fork failed
                    perror("fork");
                    exit(1);
                }
            }
        }

    return new_pixels;
}
