#ifndef MAJOR2_H
#define MAJOR2_H
#include "myhistory.h"
#include "cd.h"
#include "path.h"

#define MAX_BUFFER 1024
#define MAX_ARGS 1024
#define READ 0
#define WRITE 1


int initialize_shell();
void deinitialize_shell();
void sig_handler(int signum);

int main(int argc, char** argv);
void interactive_mode();
void batch_mode(char* filename);
void input_parse(char* input);
int process_command(char* input);
char** parse_arguments(char* input);
void execute_command(char** argv);

#endif