#ifndef MYHISTORY_H
#define MYHISTORY_H

#include <sys/queue.h>
#define MAX_HISTORY 32

struct ACommand {
    char* input;
    TAILQ_ENTRY(ACommand) entries;
};

int init_myhistory();
int deinit_myhistory();
int add_command(char* input);
int remove_command(struct ACommand* cmd);
struct ACommand* getCommand(int index);
void print_history();
void free_history();

int write_cmdIndex(int cmd_index);
int read_cmdIndex(int* cmd_index);

#endif