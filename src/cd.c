#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "cd.h"

#define READ 0
#define WRITE 1

int cwd_pipe[2] = {-1,-1};

int init_cd(char* cwd){
    // Set shell current working directory
    if (getcwd(cwd, PATH_MAX) == NULL) {
        fprintf(stderr, "failed to set shell cwd\n");
        return EXIT_FAILURE;
    }
    if(pipe(cwd_pipe)!= EXIT_SUCCESS){
        fprintf(stderr, "cwd pipe error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int deinit_cd(){
    close(cwd_pipe[READ]);
    close(cwd_pipe[WRITE]);
    return EXIT_SUCCESS;
}

int read_cwd(char* cwd)
{
    ssize_t bytesRead = read(cwd_pipe[READ], cwd, PATH_MAX - 1);
    if (bytesRead == -1) {
        fprintf(stderr, "Failed to read cmd: %s\n", strerror(errno));
        return -1;
    }
    cwd[bytesRead] = '\0';  // Null-terminate the string
    return EXIT_SUCCESS;
}

int write_cwd(char* dir)
{
    close(cwd_pipe[READ]);
    size_t dir_len = strlen(dir);
    if(dir_len > PATH_MAX - 1){
        fprintf(stderr, "write_cwd error: input too large\n");
        return EXIT_FAILURE;
    }
    else if(dir_len <= 0){
        fprintf(stderr, "write_cwd error: input empty\n");
        return EXIT_FAILURE;
    }
    if(write(cwd_pipe[WRITE],dir,dir_len) == -1){
        fprintf(stderr, "cd pipe write error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    };
    return EXIT_SUCCESS;
}

int update_cwd(char *const *argv)
{
    char* new_cwd = NULL;
    if (argv[1] == NULL || strcmp(argv[1],"~") == 0) {
        // If no argument, change to the user's HOME directory
        new_cwd = getenv("HOME");
        if (new_cwd == NULL) {
            fprintf(stderr, "HOME environment variable not set\n");
            return EXIT_FAILURE;
        }
    }
    else if(argv[1] != NULL){
        new_cwd = argv[1];
    }

    if(new_cwd == NULL){
        fprintf(stderr, "CWD is null\n");
        return EXIT_FAILURE;
    }

    return write_cwd(new_cwd);
}
