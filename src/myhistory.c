#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "myhistory.h"

#define READ 0
#define WRITE 1

struct QueueHead commandQueue;
// Define the queue head
TAILQ_HEAD(QueueHead, ACommand);

int total_commands = 0;

int cmd_pipe[2] = {-1,-1};

int init_myhistory(){
    if(pipe(cmd_pipe)!=EXIT_SUCCESS){
        fprintf(stderr, "myhistory pipe error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    TAILQ_INIT(&commandQueue);
    return EXIT_SUCCESS;
}

int deinit_myhistory(){
    free_history();
    close(cmd_pipe[READ]);
    close(cmd_pipe[WRITE]);
    return EXIT_SUCCESS;
}

int add_command(char* input){
    if(total_commands > MAX_HISTORY){
        remove_command(TAILQ_FIRST(&commandQueue));
    }

    struct ACommand* newCommand = (struct ACommand*)malloc(sizeof(struct ACommand));
    if (newCommand == NULL) {
        fprintf(stderr, "CMD allocation error\n");
        return EXIT_FAILURE;
    }
    // Copy the input to the new command's input
    newCommand->input = strdup(input);
    if (newCommand->input == NULL) {
        fprintf(stderr, "cmd data allocation error\n");
        return EXIT_FAILURE;
    }
    TAILQ_INSERT_TAIL(&commandQueue, newCommand, entries);
    total_commands++;
    return EXIT_SUCCESS;
}

int remove_command(struct ACommand* cmd){
    if(cmd == NULL){
        fprintf(stderr, "cannot remove null command\n");
        return EXIT_FAILURE; 
    }
    else{
        TAILQ_REMOVE(&commandQueue, cmd, entries);
        if (cmd->input) {
            free(cmd->input);
        }
        free(cmd);
        total_commands--;
    }
    return EXIT_SUCCESS;
}

struct ACommand* getCommand(int index){
    int current_index = 0;
    struct ACommand *cmd;
    TAILQ_FOREACH(cmd, &commandQueue, entries) {
        if (current_index == index) {
            return cmd;
        }
        current_index++;
    }
    return NULL;
}

void print_history(){
    if(TAILQ_EMPTY(&commandQueue)){
        fprintf(stderr, "empty cmd queue\n");
        return;
    }
    printf("-----Command History-----\n");
    int cmd_count = 0;
    struct ACommand *cmd;
    TAILQ_FOREACH(cmd, &commandQueue, entries) {
        printf("[%d]%s\n",cmd_count,cmd->input);
        cmd_count++;
    }
    printf("-------------------------\n");
}

void free_history(){
    while (!TAILQ_EMPTY(&commandQueue)) {
        struct ACommand *cmd = TAILQ_FIRST(&commandQueue);
        if(cmd){
            remove_command(cmd);
        }
    }
    TAILQ_INIT(&commandQueue);
}

int write_cmdIndex(int cmd_index){
    close(cmd_pipe[READ]);
    // write the path to the pipe
    if(write(cmd_pipe[WRITE],&cmd_index,sizeof(cmd_index)) == -1){
        fprintf(stderr, "cmd pipe write error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    };
    return EXIT_SUCCESS;
}

int read_cmdIndex(int* cmd_index){
    ssize_t bytesRead = read(cmd_pipe[READ],cmd_index,sizeof(int));
    if (bytesRead == -1) {
        fprintf(stderr, "failed to read cmd: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}