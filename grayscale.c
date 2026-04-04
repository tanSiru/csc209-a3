#include <stdio.h>
#include <stdlib.h>
#include "read_bmp.h"
#include <math.h>
#include "encoding.h"
#include <unistd.h>

// https://github.com/cmartinezal/image-filter/blob/main/helpers.c



struct pixel *apply_gray_to_row(struct pixel *row, int width){
        struct pixel *new_row = malloc(width*sizeof(struct pixel));

        if(new_row == NULL){
                return NULL;
        }

        for(int i = 0;i < width; i++){
                struct pixel p = row[i];

                unsigned char red = p.red;
                unsigned char green = p.green;
                unsigned char blue = p.blue;

                int average = round((red+green+blue)/3.0);

                new_row[i].red = average;
                new_row[i].green = average;
                new_row[i].blue = average;
        }

        return new_row;
}

struct pixel **apply_grayscale(struct pixel **pixel_array, int height, int width){
        

        /*
        pipe_fd[i][0] parent -> child
        pipe_fd[i][1] child -> parent


        pipe_fd[i][][0] -> read fd
        pipe_fd[i][][1] -> write fd
        */ 
        
        int pipe_fd[height][2][2];
        pid_t child_pids[height]; // keep an array of child pids so later we can use waitpid to synchronize wait calls

        for(int i = 0; i <height; i++) {
                if(pipe(pipe_fd[i][0])==-1){
                        // ASK HOW WE WILL HANDLE ERROR
                        perror("pipe");
                        exit(1);
                }

                if(pipe(pipe_fd[i][1])==-1){
                        perror("pipe");
                        exit(1);
                }

                pid_t result = fork();


                struct pixel *cur_row = pixel_array[i];

                if(result>0){
                        close(pipe_fd[i][1][1]); // close the pipe child->parent for writing
                        close(pipe_fd[i][0][0]); // close the pipe parent->child for reading

                        child_pids[i] = result; // store the pid of the child responsible for row i, allowing us to wait for the correct child for pipe_fd[i]

                        // malloc enough space for Message and the size of each row 
                        Message *row_to_send = malloc(sizeof(Message) + width*sizeof(struct pixel));
                        if(row_to_send==NULL){
                                perror("malloc");
                                exit(1);
                        }
                        row_to_send->row = i;

                        // copy the pixels individually since the pixels sent through pipe must be the raw bytes rather than the pointer
                        for(int j = 0; j<width; j++){
                                row_to_send->pixel_row[j] = cur_row[j];
                        }

                        // write the encoded data into pipe
                        if(write(pipe_fd[i][0][1], row_to_send,(sizeof(Message)+width*sizeof(struct pixel)))==-1){
                                perror("write");
                                exit(1);
                        }
                        
                        free(row_to_send);
                        close(pipe_fd[i][0][1]); // close write pipe after everything has been written
                }


                if(result < 0) {
                        printf("Error");
                        exit(1);
                } else if (result == 0) {
                        close(pipe_fd[i][1][0]); // close the pipe child->parent for reading
                        close(pipe_fd[i][0][1]); // close the pipe parent->child for writing

                        // malloc enough space for Message and the size of each row 
                        Message *received_row = malloc(sizeof(Message) + width*sizeof(struct pixel));
                        if(received_row==NULL){
                                perror("malloc");
                                exit(1);
                        }

                        // read data sent through pipe
                        if(read(pipe_fd[i][0][0], received_row, (sizeof(Message)+width*sizeof(struct pixel)))==-1){
                                perror("read");
                                exit(1);
                        }

                        // parse the data
                        int row_num = received_row->row;
                        struct pixel *row = received_row->pixel_row;

                        // call function to apply gray to row
                        struct pixel *gray_row = apply_gray_to_row(row, width);


                        // message to be sent back
                        Message *msg = malloc(sizeof(Message) + width*sizeof(struct pixel));
                        if(msg==NULL){
                                perror("malloc");
                                exit(1);
                        }

                        // copy the data individually since the data sent through pipe must be the raw bytes rather than the pointer
                        msg->row = row_num;
                        for(int j = 0; j<width; j++){
                                msg->pixel_row[j] = gray_row[j];
                        }

                        // write the row which has been applied the gray filter into pipe
                        if(write(pipe_fd[i][1][1], msg, (sizeof(Message)+width*sizeof(struct pixel)))==-1){
                                perror("write");
                                exit(1);
                        }


                        // free all the memories for the malloc calls
                        free(gray_row);
                        free(msg);
                        free(received_row);
                        // close all the pipes as the child has no more work to do
                        close(pipe_fd[i][1][1]); 
                        close(pipe_fd[i][0][0]); 
                        exit(0);
                }
        }

        // stores the reconstructed pixel array after every row had a gray filter applied to it
        struct pixel **modified_pixel_array = malloc(height*sizeof(struct pixel *));

        //
        for(int i = 0; i < height; i++) {
                Message *received_msg = malloc(sizeof(Message) + width*sizeof(struct pixel));
                if(received_msg==NULL){
                        perror("malloc");
                        exit(1);
                }
                int status;
                // use the child_pids array from earlier to synchronize the child process with the corresponding pipe
                pid_t childpid = waitpid(child_pids[i], &status, 0);
                if(childpid == -1){
                        perror("wait");
                }
                if(WIFEXITED(status)){
                        if(WEXITSTATUS(status)==0){
                                if(read(pipe_fd[i][1][0], received_msg, (sizeof(Message)+width*sizeof(struct pixel)))==-1){
                                        perror("read");
                                        exit(1);
                                }

                                int num_row = received_msg->row;

                                modified_pixel_array[num_row] = malloc(width*sizeof(struct pixel));

                                for(int j = 0; j < width; j++){
                                        modified_pixel_array[num_row][j] = received_msg->pixel_row[j];
                                }

                                close(pipe_fd[i][1][0]);
                        }else{
                                // DISCUSS AND HANDLE ERROR 
                        }
                }
        }
        return modified_pixel_array;
}

