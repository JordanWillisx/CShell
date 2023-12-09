#ifndef PATH_H
#define PATH_H

int init_path();
int deinit_path();

int remove_path(char* pathname);
void print_paths();
int add_path(char* input);
int remove_path(char* input);
int write_path(char* input);
int read_path();

#endif