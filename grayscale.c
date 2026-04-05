#include <stdio.h>
#include <stdlib.h>
#include "read_bmp.h"
#include <math.h>
#include "encoding.h"
#include <unistd.h>
#include <sys/wait.h>

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

Message *make_message(struct pixel *row, int row_num, int width){
        // malloc enough space for Message and the size of each row 
        Message *msg = malloc(sizeof(Message) + width*sizeof(struct pixel));
        if(msg == NULL){
                return NULL;
        }

        msg->row = row_num;

        // copy the pixels individually since the pixels sent through pipe must be the raw bytes rather than the pointer
        for(int j = 0; j < width; j++){
                msg->pixel_row[j] = row[j];
        }

        return msg;
}

int write_message(int fd, Message *msg, int width){
        /*
        write entire messages using write loop to ensure everything is written
        just using write() may only write small chunks rather than entire message
        */
        int total = 0;
        int size = sizeof(Message) + width*sizeof(struct pixel);

        while(total < size){
                int bytes_written = write(fd, ((char *)msg) + total, size - total);
                if(bytes_written <= 0){
                        return -1;
                }
                total += bytes_written;
        }

        return 0;
}

Message *read_message(int fd, int width){
        /*
        read entire messages using write loop to ensure everything is written
        just using write() may only write small chunks rather than entire message
        */

        // malloc enough space for Message and the size of each row 
        Message *msg = malloc(sizeof(Message) + width*sizeof(struct pixel));
        if(msg == NULL){
                return NULL;
        }

        // read data sent through pipe
        int total = 0;
        int size = sizeof(Message) + width*sizeof(struct pixel);

        // read data sent through pipe
        while(total < size){
                int bytes_read = read(fd, ((char *)msg) + total, size - total);
                if(bytes_read <= 0){
                        free(msg);
                        return NULL;
                }
                total += bytes_read;
        }
        return msg;
}

int child_process_row(int read_fd, int write_fd, int width){
        Message *received_row = read_message(read_fd, width);
        if(received_row == NULL){
                perror("read");
                return 1;
        }

        // parse the data
        int row_num = received_row->row;
        struct pixel *row = received_row->pixel_row;

        // call function to apply gray to row
        struct pixel *gray_row = apply_gray_to_row(row, width);
        if(gray_row == NULL){
                perror("malloc");
                free(received_row);
                return 1;
        }


        // message to be sent back
        Message *msg = make_message(gray_row, row_num, width);
        if(msg == NULL){
                perror("malloc");
                free(gray_row);
                free(received_row);
                return 1;
        }

        // write the row which has been applied the gray filter into pipe
        if(write_message(write_fd, msg, width) == -1){
                perror("write");
                free(gray_row);
                free(received_row);
                free(msg);
                return 1;
        }

        // free all the memories for the malloc calls
        free(gray_row);
        free(msg);
        free(received_row);

        return 0;
}

void copy_message_into_array(struct pixel **modified_pixel_array, Message *received_msg, int width){
        int num_row = received_msg->row;

        modified_pixel_array[num_row] = malloc(width*sizeof(struct pixel));
        if(modified_pixel_array[num_row] == NULL){
                perror("malloc");
                exit(1);
        }

        for(int j = 0; j < width; j++){
                modified_pixel_array[num_row][j] = received_msg->pixel_row[j];
        }
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
                        printf("parent created child for row %d\n", i);
                        fflush(stdout);

                        close(pipe_fd[i][1][1]); // close the pipe child->parent for writing
                        close(pipe_fd[i][0][0]); // close the pipe parent->child for reading

                        child_pids[i] = result; // store the pid of the child responsible for row i, allowing us to wait for the correct child for pipe_fd[i]

                        // call helper function to make the message that will be sent to pipe
                        Message *row_to_send = make_message(cur_row, i, width);
                        if(row_to_send == NULL){
                                perror("malloc");
                                exit(1);
                        }

                        // write the encoded data into pipe
                        if(write_message(pipe_fd[i][0][1], row_to_send, width) == -1){
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
                        printf("child processing row %d\n", i);
                        fflush(stdout);
                        close(pipe_fd[i][1][0]); // close the pipe child->parent for reading
                        close(pipe_fd[i][0][1]); // close the pipe parent->child for writing

                        

                        // call helper function to handle processing the row and sending the processed row to pipe
                        int child_status = child_process_row(pipe_fd[i][0][0], pipe_fd[i][1][1], width);
                
                        // close all the pipes as the child has no more work to do
                        close(pipe_fd[i][1][1]); 
                        close(pipe_fd[i][0][0]); 
                        printf("child done row %d\n", i);
                        exit(child_status);
                }
        }

        // stores the reconstructed pixel array after every row had a gray filter applied to it
        struct pixel **modified_pixel_array = malloc(height*sizeof(struct pixel *));

        //
        for(int i = 0; i < height; i++) {
                int status;
                // use the child_pids array from earlier to synchronize the child process with the corresponding pipe
                printf("parent waiting for row %d\n", i);
                pid_t childpid = waitpid(child_pids[i], &status, 0);
                printf("parent finished waiting for row %d\n", i);
                if(childpid == -1){
                        perror("wait");
                }
                if(WIFEXITED(status) && WEXITSTATUS(status)==0){
                        // read in the processed row by calling helper function
                        printf("parent reading row %d\n", i);
                        Message *received_msg = read_message(pipe_fd[i][1][0], width);
                        if(received_msg == NULL){
                                perror("read");
                                exit(1);
                        }

                        // write the processed row into modified_pixel_array using helper function
                        copy_message_into_array(modified_pixel_array, received_msg, width);

                        free(received_msg);

                        close(pipe_fd[i][1][0]);
                }else{
                        /*
                        pipe_fd[0][0] parent -> child
                        pipe_fd[1][1] child -> parent


                        pipe_fd[i][][0] -> read fd
                        pipe_fd[i][][1] -> write fd
                        */ 

                        /*
                        failure due to exit or signalling will both be handled in the same way
                        handle failure by creating a new child to work on that row
                        the logic is pretty much the exact same so the code is basically copied over
                        */
                        if(pipe(pipe_fd[i][0])==-1){
                                perror("pipe");
                                exit(1);
                        }

                        if(pipe(pipe_fd[i][1])==-1){
                                perror("pipe");
                                exit(1);
                        }

                        pid_t new_child = fork();

                        struct pixel *cur_row = pixel_array[i];

                        if(new_child > 0){
                                close(pipe_fd[i][1][1]); // close child->parent write
                                close(pipe_fd[i][0][0]); // close parent->child read

                                child_pids[i] = new_child;

                                Message *row_to_send = make_message(cur_row, i, width);
                                if(row_to_send == NULL){
                                        perror("malloc");
                                        exit(1);
                                }

                                if(write_message(pipe_fd[i][0][1], row_to_send, width) == -1){
                                        perror("write");
                                        exit(1);
                                }

                                free(row_to_send);
                                close(pipe_fd[i][0][1]);

                                // wait again for replacement child
                                int new_status;
                                pid_t childpid2 = waitpid(new_child, &new_status, 0);
                                if(childpid2 == -1){
                                        perror("wait");
                                        exit(1);
                                }

                                if(WIFEXITED(new_status) && WEXITSTATUS(new_status) == 0){
                                        Message *received_msg = read_message(pipe_fd[i][1][0], width);
                                        if(received_msg == NULL){
                                                perror("read");
                                                exit(1);
                                        }

                                        copy_message_into_array(modified_pixel_array, received_msg, width);

                                        free(received_msg);
                                        close(pipe_fd[i][1][0]);
                                }else{
                                        fprintf(stderr, "Row %d failed again\n", i);
                                        exit(1);
                                }
                        }

                        if(new_child < 0){
                                perror("fork");
                                exit(1);
                        } else if(new_child == 0){
                                close(pipe_fd[i][1][0]);
                                close(pipe_fd[i][0][1]);

                                int child_status = child_process_row(pipe_fd[i][0][0], pipe_fd[i][1][1], width);

                                close(pipe_fd[i][1][1]);
                                close(pipe_fd[i][0][0]);
                                exit(child_status);
                        }
                }
        }
        return modified_pixel_array;
}

