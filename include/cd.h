#ifndef CD_H
#define CD_H


int init_cd(char* cwd);
int deinit_cd();
int read_cwd(char* cwd);
int write_cwd(char* dir);
int update_cwd(char *const *argv);

#endif