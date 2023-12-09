#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>
#include "major2.h"

char* username;
char hostname[HOST_NAME_MAX];
char cwd[PATH_MAX];
long maxArgLength = PATH_MAX * 512; // 2mb
time_t start_time;

#define CWD_MOVE_SEMAPHORE "/cwd_move"
sem_t* cwd_move_semaphore = NULL;
#define PATH_ADD_SEMAPHORE "/path_add"
sem_t* path_add_semaphore = NULL;
#define PATH_REMOVE_SEMAPHORE "/path_remove"
sem_t* path_remove_semaphore = NULL;
#define HISTORY_EXEC_SEMAPHORE "/history_exec"
sem_t* history_exec_semaphore = NULL;
#define HISTORY_CLEAR_SEMAPHORE "/history_clear"
sem_t* history_clear_semaphore = NULL;

bool isWhitespace(const char *str) {
    while (*str != '\0') {
        if (!isspace(*str)) {
            // If a non-whitespace character is found, return false
            return false;
        }
        str++;
    }
    // If the loop completes without finding non-whitespace characters, return true
    return true;
}

int create_semaphore(sem_t **sem, const char *sem_name) {
    *sem = sem_open(sem_name, O_CREAT, 0666, 0); // Create a semaphore
    if (*sem == SEM_FAILED) {
        fprintf(stderr, "semaphore creation error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int initialize_shell(){
    // Set the start time of the shell
    start_time = time(NULL);
    
    maxArgLength = sysconf(_SC_ARG_MAX);
    if (maxArgLength == -1) {
        fprintf(stderr, "sysconf argMaxlength error\n");
        return EXIT_FAILURE;
    }
    // Set up signal handler
    struct sigaction sa_sig;
    sa_sig.sa_handler = sig_handler;
    sa_sig.sa_flags = 0;
    sigemptyset(&sa_sig.sa_mask);

    if (sigaction(SIGINT, &sa_sig, NULL) == -1) {
        fprintf(stderr, "sigaction(SIGINT) error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction(SIGTERM, &sa_sig, NULL) == -1) {
        fprintf(stderr, "sigaction(SIGTERM) error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (sigaction(SIGUSR1, &sa_sig, NULL) == -1) {
        fprintf(stderr, "sigaction(SIGUSR1) error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Automatically reap child processes to prevent zombies
    signal(SIGCHLD, SIG_IGN);

    if(create_semaphore(&cwd_move_semaphore, CWD_MOVE_SEMAPHORE) != EXIT_SUCCESS){
        return EXIT_FAILURE;
    };
    if(create_semaphore(&path_add_semaphore, PATH_ADD_SEMAPHORE) != EXIT_SUCCESS){
        return EXIT_FAILURE;
    };
    if(create_semaphore(&path_remove_semaphore, PATH_REMOVE_SEMAPHORE) != EXIT_SUCCESS){
        return EXIT_FAILURE;
    };
    if(create_semaphore(&history_exec_semaphore, HISTORY_EXEC_SEMAPHORE) != EXIT_SUCCESS){
        return EXIT_FAILURE;
    };
    if(create_semaphore(&history_clear_semaphore, HISTORY_CLEAR_SEMAPHORE) != EXIT_SUCCESS){
        return EXIT_FAILURE;
    };

    
    // Set the shell username
    username = getlogin();
    if (username == NULL) {
        username = getenv("USER");
        // set default username
        if (username == NULL) {
            fprintf(stderr, "Setting default username.\n");
            username = "user";
        }
    }
    // Set the shell hostname
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        fprintf(stderr, "failed to gethostname\n");
        username = "pc";
    }
    
    if(init_myhistory() == EXIT_FAILURE){ 
        return EXIT_FAILURE;
    };

    if(init_cd(cwd) == EXIT_FAILURE){ 
        return EXIT_FAILURE;
    };
    
    if(init_path() == EXIT_FAILURE){ 
        return EXIT_FAILURE;
    };

    return EXIT_SUCCESS;

}

void cleanup_semaphore(sem_t *sem, const char *sem_name) {
    if (sem_close(sem) == -1) {
        fprintf(stderr, "close semaphore error: %s\n", strerror(errno));
    }

    if (sem_unlink(sem_name) == -1) {
        fprintf(stderr, "semaphore unlink error: %s\n", strerror(errno));
    }
}

void deinitialize_shell(){
    deinit_myhistory();
    deinit_cd();
    deinit_path();
    if(cwd_move_semaphore != NULL){cleanup_semaphore(cwd_move_semaphore,CWD_MOVE_SEMAPHORE);}
    if(path_add_semaphore != NULL){cleanup_semaphore(path_add_semaphore,PATH_ADD_SEMAPHORE);}
    if(path_remove_semaphore != NULL){cleanup_semaphore(path_remove_semaphore,PATH_REMOVE_SEMAPHORE);}
    if(history_exec_semaphore != NULL){cleanup_semaphore(history_exec_semaphore,HISTORY_EXEC_SEMAPHORE);}
    if(history_clear_semaphore != NULL)cleanup_semaphore(history_clear_semaphore,HISTORY_CLEAR_SEMAPHORE);
    exit(EXIT_SUCCESS);
}

void execute_history(){
    int cmdIndex;
    if(read_cmdIndex(&cmdIndex) != EXIT_SUCCESS){
        return;
    };
    struct ACommand* cmd = getCommand(cmdIndex);
    if(cmd->input != NULL){
        process_command(cmd->input);
    }
}

void sig_handler(int signum){
    //Signal Handling Here
    if(signum == SIGUSR1){ 
        if (sem_trywait(history_exec_semaphore) == 0){ execute_history(); }
        if (sem_trywait(history_clear_semaphore) == 0){ free_history(); }
        if (sem_trywait(cwd_move_semaphore) == 0) { //
            char tempCWD[PATH_MAX];
            if (read_cwd(tempCWD) != EXIT_SUCCESS) {
                fprintf(stderr, "Failed to read current working directory\n");
                return;
            }
            if (chdir(tempCWD) != EXIT_SUCCESS) {
                fprintf(stderr, "cd error: %s\n",strerror(errno));
                return;
            }
            // update terminal cwd
            if (getcwd(cwd, PATH_MAX) == NULL) {
                fprintf(stderr, "failed to get cwd %s\n",cwd);
            }   
        }
        if (sem_trywait(path_add_semaphore) == 0){ 
            char path[PATH_MAX];
            if(read_path(path) == EXIT_SUCCESS){
                add_path(path);
            }
        }
        if (sem_trywait(path_remove_semaphore) == 0){ 
            char path[PATH_MAX];
            if(read_path(path) == EXIT_SUCCESS){
                remove_path(path);
            }
        }
    }
    else if(signum == SIGTERM){ deinitialize_shell(); }
    else{ printf("\n"); }
}

int main(int argc, char** argv){
    if (initialize_shell() != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize shell\n");
    }
    else if(argc == 2){
        batch_mode(argv[1]);
    }
    else if(isatty(STDIN_FILENO)){
        interactive_mode();
    }
    else{
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    }
    deinitialize_shell();
    return 0;
}

void interactive_mode(){
    while (true) {
        char input[MAX_BUFFER];
        // print the shell prompt
        printf("%s@%s:%s$ ", username, hostname, cwd);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin)) {
                //end-of-file (Ctrl+D)
                printf("Exiting shell.\n");
                break;
            } else {
                //perror("Error reading input");
                continue;
            }
        }
        
        size_t input_len = strlen(input);
        
        if (input_len >= MAX_BUFFER - 1) {
            fprintf(stderr, "input too large\n");
            continue;
        }
        
        if (input_len <= 1 || strcmp(input, "\n") == 0) {
            continue;
        }
        
        // Remove the trailing newline character
        
        input[strcspn(input, "\n")] = '\0';
        
        printf("%s\n", input);
        
        input_parse(input);
    }
}

void batch_mode(char* filename){
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "file open error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    char input[MAX_BUFFER];
    while (fgets(input, sizeof(input), file) != NULL) {
        // Remove the trailing newline character
        input[strcspn(input, "\n")] = '\0';
        printf("%s\n", input);       
        input_parse(input);
    }
    fclose(file);
}

void input_parse(char* input){
    const char line_delimiters[] = ";";
    const char pipe_delimiters[] = "|";

    char* token;
    char* line_saveptr = NULL;
    char* pipe_saveptr = NULL;
    
    token = strtok_r(input, line_delimiters, &line_saveptr);
    while (token != NULL) {
        char* inner_token = strtok_r(token, pipe_delimiters, &pipe_saveptr);
        while (inner_token != NULL) {
            if(!isWhitespace(inner_token) && add_command(inner_token) == EXIT_SUCCESS){
                process_command(inner_token);
            }else{
                fprintf(stderr, "Parse Error\n");
            }
            inner_token = strtok_r(NULL, pipe_delimiters,&pipe_saveptr);
        }
        token = strtok_r(NULL, line_delimiters, &line_saveptr);
    }
    return;
}

int next_pipefd[2] = {-1,-1};
int prev_pipefd[2]= {-1,-1};

int process_command(char* input){
    if(pipe(next_pipefd) < 0){
        fprintf(stderr, "Process Pipe Error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    pid_t pid = fork();
    if (pid < 0) // fork error
    {
        fprintf(stderr, "Failed to fork sub-process\n");
        close(next_pipefd[READ]);
        close(next_pipefd[WRITE]);
        return EXIT_FAILURE;
    }
    else if (pid == 0) // Child Process
    {
        close(next_pipefd[READ]);
        /*if (dup2(next_pipefd[WRITE], STDOUT_FILENO) < 0) {// Redirect stdout to the write end of the pipe
            fprintf(stderr, "STDOUT dup2 Error: %s\n", strerror(errno));
            fprintf(stderr, "pipefd[WRITE]: %d\n", next_pipefd[WRITE]);
            exit(EXIT_FAILURE);
        }
        // Redirect stdin to the read end of the previous pipe
        if (prev_pipefd[READ] != -1) {
            if (dup2(prev_pipefd[READ], STDIN_FILENO) < 0){
                fprintf(stderr, "STDIN dup2 Error: %s\n", strerror(errno));
                fprintf(stderr, "prev_pipefd[READ]: %d\n", prev_pipefd[READ]);
                exit(EXIT_FAILURE);
            }
            close(prev_pipefd[READ]);
            close(prev_pipefd[WRITE]);
        }*/
        //char** tokens = tokenize(input," ");
        char** argv = parse_arguments(input);
        if(argv != NULL){
            execute_command(argv);
        }else{
            fprintf(stderr, "couldn't parse arguments\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        close(next_pipefd[WRITE]);
        int status = -1;
        waitpid(pid, &status, 0);

        // Child Process Signal/Exit Status
        if (WIFEXITED(status)) {// If Child Process has exitted
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "Child process exited with non-zero status %d\n", WEXITSTATUS(status));
            }
        } 
        else if (WIFSIGNALED(status)) { // If Child Process was signaled
            fprintf(stderr, "Child process terminated by signal %d\n", WTERMSIG(status));
        }

        if (prev_pipefd[READ] != -1) {
            close(prev_pipefd[READ]);
            close(prev_pipefd[WRITE]);
        }
        prev_pipefd[READ] = next_pipefd[READ];
        prev_pipefd[WRITE] = next_pipefd[WRITE];
        return status;
    }
    return EXIT_SUCCESS;
}

void free_args(char** argv){
    if(argv == NULL){
        fprintf(stderr, "cannot free null argv array\n");
        return;
    }
    int i;
    for (i = 0; argv[i] != NULL; i++)
    {
        free(argv[i]);
    }
    free(argv);
}

char** parse_arguments(char* input){
    //TODO: SETUP REDIRECTION IN HERE
    const char whitespace_delimiters[] = " ";
    char** argv = (char**)malloc(maxArgLength);
    int token_count = 0;
    char* token_saveptr = NULL;
    char* token = strtok_r(input, whitespace_delimiters, &token_saveptr);
    while (token != NULL) {
        if (strcmp(token,">")== 0)
        {
            while(strcmp(token,">") == 0){
                token = strtok_r(NULL, whitespace_delimiters, &token_saveptr);
            }
            if(token == NULL){
                fprintf(stderr, "syntax error near >\n");
                free_args(argv);
                return NULL;
            }
            int file = open(token,O_WRONLY|O_CREAT,0777);

            if (file < 0) {
                fprintf(stderr, "Error opening %s\n",token);
                free_args(argv);
                return NULL;
            }
            // Redirect stdout to the file
            if (dup2(file,STDOUT_FILENO) < 0) {
                fprintf(stderr, "redirection error\n");
                free_args(argv);
                return NULL;
            }
            close(file);
        }
        else if (strcmp(token,"<") == 0)
        {
            printf("BCK REDIRECTION\n");
        }else{
            argv[token_count] = strdup(token);
            if(argv[token_count] == NULL){
                fprintf(stderr, "argv memory allocation error\n");
                free_args(argv);
                return NULL;
            }
            token_count++;
        }
        token = strtok_r(NULL, whitespace_delimiters, &token_saveptr);
    }
    char** resultArgv = (char**)realloc(argv,(token_count + 1)* sizeof(char*));
    if(resultArgv == NULL){
        free_args(argv);
        return NULL;
    }else{
        resultArgv[token_count] = NULL; // null terminate resultArgv array
        return resultArgv;
    }
}

void execute_command(char** argv){
    if(argv[0] == NULL) {
        exit(EXIT_FAILURE);
    }
    else if(strcmp(argv[0], "cd") == 0){
        if(update_cwd(argv) == EXIT_SUCCESS){
            sem_post(cwd_move_semaphore);
            kill(getppid(),SIGUSR1);
        };
    }
    else if(strcmp(argv[0], "myhistory") == 0){
        if (argv[1] == NULL) {
            print_history();
        }
        else if(strcmp(argv[1], "-e") == 0 || argv[2] != NULL){
            int cmd_index = atoi(argv[2]);
            sem_post(history_exec_semaphore);
            write_cmdIndex(cmd_index);
            kill(getppid(),SIGUSR1);   
        }
        else if(strcmp(argv[1], "-c") == 0){
            sem_post(history_clear_semaphore);
            kill(getppid(),SIGUSR1);
        }
        else{
            printf("myhistory <usage>\n");
        }
    }
    else if(strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0){
        printf("exiting...!\n"); 
        kill(getppid(), SIGTERM);
    }
    else if (strcmp(argv[0], "path") == 0){
        if(argv[1] != NULL){
            if(strcmp(argv[1], "-p") == 0){
                print_paths();
            }
            else if(strcmp(argv[1], "+") == 0 && argv[2] != NULL){
                if(write_path(argv[2]) == EXIT_SUCCESS){
                    sem_post(path_add_semaphore);
                    kill(getppid(),SIGUSR1);
                }
            }
            else if(strcmp(argv[1], "-") == 0 && argv[2] != NULL){
                if(write_path(argv[2]) == EXIT_SUCCESS){
                    sem_post(path_remove_semaphore);
                    kill(getppid(),SIGUSR1);
                }
            };
        }else{
            char* path = getenv("PATH");
            if(path != NULL){
                printf("%s\n",path);
            }
        }
    }
    else if (strcmp(argv[0], "time") == 0){
        time_t current_time = time(NULL);
        if(argv[1] != NULL && strcmp(argv[1], "-d") == 0){
            // Calculate and print the running time
            double elapsed_time = difftime(current_time, start_time);
            printf("Shell has been running for %.2f seconds\n", elapsed_time);
        }
        printf("%s", ctime(&current_time));
    }
    else{ execvp(argv[0], argv); }
    free_args(argv);
    exit(EXIT_SUCCESS);
}