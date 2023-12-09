#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "path.h"

#define READ 0
#define WRITE 1

char* initial_path;

int path_pipe[2] = {-1,-1};

const char path_delimiters[] = ":";

int init_path()
{
    if(pipe(path_pipe) != EXIT_SUCCESS){
        fprintf(stderr, "path pipe error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    char* env_path = getenv("PATH");
    if(env_path != NULL){
        initial_path = strdup(env_path);
        if(initial_path == NULL){
            fprintf(stderr, "inital path allocation error\n");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int deinit_path()
{
    if(initial_path != NULL){
        if (setenv("PATH", initial_path, 1) != EXIT_SUCCESS) {
            fprintf(stderr, "couldn't set envpath\n");
        }
        free(initial_path);
    }
    close(path_pipe[READ]); close(path_pipe[WRITE]);
    return EXIT_SUCCESS;
}

void print_paths()
{
    char* env_path = getenv("PATH");
    if(env_path == NULL){
        fprintf(stderr, "couldnt't get path env\n");
        return;
    }
    char* envpath_cpy = strdup(env_path);
    if(envpath_cpy == NULL){
        fprintf(stderr, "parse envpath allocation error\n");
        return;
    }
    int path_count = 0;
    char* path_saveptr = NULL;
    char* pathToken = strtok_r(envpath_cpy, path_delimiters, &path_saveptr);
    printf("-----ENVPATH LIST-----\n");
    while (pathToken != NULL) {
        printf("[%d]%s\n",path_count,pathToken);
        pathToken = strtok_r(NULL, path_delimiters, &path_saveptr);
        path_count++;
    }
    printf("----------------------\n");
}

int add_path(char* input){
    size_t path_len = strlen(input);
    if(path_len > PATH_MAX){
        fprintf(stderr, "path too large\n");
        return EXIT_FAILURE;
    }
    char* env_path = getenv("PATH");
    if(env_path == NULL){
        fprintf(stderr, "couldnt't get path env\n");
        return EXIT_FAILURE;
    }
    char* envpath_cpy = strdup(env_path);
    if(envpath_cpy == NULL){
        fprintf(stderr, "parse envpath allocation error\n");
        return EXIT_FAILURE;
    }
    char* path_saveptr = NULL;
    char* pathToken = strtok_r(envpath_cpy, path_delimiters, &path_saveptr);
    int pathToken_count = 0;
    while (pathToken != NULL) {
        if(strcmp(pathToken,input) == 0){
            fprintf(stderr, "path already exists\n");
            return EXIT_FAILURE;
        }
        pathToken = strtok_r(NULL, path_delimiters, &path_saveptr);
        pathToken_count++;
    }
    if(pathToken_count > 0){strcat(env_path,":");}
    strcat(env_path,input);
    if (setenv("PATH", env_path, 1) != EXIT_SUCCESS) {
        fprintf(stderr, "Error setting PATH env\n");
        return EXIT_FAILURE;
    }
    free(envpath_cpy);
    printf("ADDED %d paths\n",pathToken_count);
    return EXIT_SUCCESS;
}

int remove_path(char* input){
    char new_envPath[PATH_MAX * 512];
    char* env_path = getenv("PATH");
    if(env_path == NULL){
        fprintf(stderr, "couldnt't get path env\n");
        return EXIT_FAILURE;
    }
    char* envpath_cpy = strdup(env_path);
    if(envpath_cpy == NULL){
        fprintf(stderr, "parse envpath allocation error\n");
        return EXIT_FAILURE;
    }
    int pathToken_count = 0;
    char* path_saveptr = NULL;
    char* pathToken = strtok_r(envpath_cpy, path_delimiters, &path_saveptr);
    while (pathToken != NULL) {
        if( strcmp(pathToken,input) != 0 ){
            if(pathToken > 0){strcat(new_envPath,":");}
            strcat(new_envPath,pathToken);
            pathToken_count++;
        }
        pathToken = strtok_r(NULL, path_delimiters, &path_saveptr);
    }
    if (setenv("PATH", new_envPath, 1) != EXIT_SUCCESS) {
        fprintf(stderr, "Error setting PATH env\n");
        return EXIT_FAILURE;
    }
    free(envpath_cpy);
    return EXIT_SUCCESS;
}

int write_path(char* input){
    close(path_pipe[READ]);
    if(write(path_pipe[WRITE],input,strlen(input)) == -1){
        fprintf(stderr, "path pipe write error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    };
    return EXIT_SUCCESS;
}

int read_path(char* path){
    ssize_t bytesRead = read(path_pipe[READ], path, PATH_MAX - 1);
    if (bytesRead == -1) {
        fprintf(stderr, "Failed to read cmd: %s\n", strerror(errno));
        return -1;
    }
    path[bytesRead] = '\0';  // Null-terminate the string
    return EXIT_SUCCESS;
}
