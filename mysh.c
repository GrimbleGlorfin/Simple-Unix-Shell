// mysh - a simple Unix shell

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

int main(void) {
    // Declare line var
    char line[1024];
    char *args[64];
    int arg_count;
    char *output_file_name;
    char *input_file_name;
    int pipefd[2];
    char *left_pipe[64];
    char *right_pipe[64];


    // Main loop
    while (true) {
        output_file_name = NULL;
        input_file_name = NULL;
        bool pipe_handled = false;
        //init input
        printf("mysh> ");
        fflush(stdout);
        //break if NULL
        if (fgets(line, sizeof(line), stdin) == NULL) {
            return 0;
        }
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        //reset arg_count
        arg_count = 0;
        args[arg_count] = strtok(line, " ");
        //split tokens
        while (args[arg_count] != NULL) {
            arg_count++;
            args[arg_count] = strtok(NULL, " ");
        }
        //check for empty prompt
        if (arg_count == 0) {
            continue;
        }

        //check for 'cd' command
        if (strcmp(args[0], "cd") == 0) {
            //check for just cd
            if (args[1] == NULL) { //redirect to Home
                chdir(getenv("HOME"));
            } else if (chdir(args[1]) == -1) {
                //run chdir and check for error
                perror("cd failed");
            }
            //continue so fork doesn't happen
            continue; 
        }

        //check for 'exit' command
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        //check for > or < to redirect output/input
        for (int i = 0; i < arg_count; i++) {
            //Check for output redirection ">"
            if (args[i] == NULL) continue;
            if (strcmp(args[i], ">") == 0) {
                if (args[i+1] == NULL) {
                    fprintf(stderr, "missing filename after >\n");
                    break;
                }
                //get filename
                output_file_name = args[i+1];
                //clear > and filename
                args[i] = NULL;
                args[i+1] = NULL;
                break;
            }
            //check for input redirection "<"
            if (args[i] == NULL) continue;
            if (strcmp(args[i], "<") == 0) {
                if (args[i+1] == NULL) {
                    fprintf(stderr, "missing filename after <\n");
                    break;
                }
                //get filename
                input_file_name = args[i+1];
                //clear < and filename
                args[i] = NULL;
                args[i+1] = NULL;
                break;
            }
        }
        
        //check for | for piping
        for (int i = 0; i < arg_count; i++) {
            if (args[i] == NULL) continue;
            if (strcmp(args[i], "|") == 0) {
                args[i] = NULL;
                for (int j = 0; j < i; j++) {
                    left_pipe[j] = args[j];
                }
                left_pipe[i] = NULL;
                int right_len = 0;
                while(args[i + 1 + right_len] != NULL) right_len++;
                for (int j = 0; j < right_len; j++) {
                    right_pipe[j] = args[j + i + 1];
                }
                right_pipe[right_len] = NULL;
                
                if (pipe(pipefd) == -1) {
                    perror("pipe");
                    exit(EXIT_FAILURE);
                }
                
                //left fork
                pid_t pid1 = fork();

                if (pid1 == 0) {  //check if child
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                    execvp(left_pipe[0], left_pipe);
                    //if execvp fails - print error and exit
                    perror("execvp failed");
                    exit(1);
                } else if (pid1 < 0) { //check if failed
                    perror("fork failed");
                }

                //right fork
                pid_t pid2 = fork();

                if (pid2 == 0) {  //check if child
                    close(pipefd[1]);
                    dup2(pipefd[0], STDIN_FILENO);
                    close(pipefd[0]);
                    execvp(right_pipe[0], right_pipe);
                    //if execvp fails - print error and exit
                    perror("execvp failed");
                    exit(1);
                } else if (pid2 < 0) { //check if failed
                    perror("fork failed");
                }
                
                //parent closes everything
                close(pipefd[0]);
                close(pipefd[1]);
                waitpid(pid1, NULL, 0);
                waitpid(pid2, NULL, 0);
                pipe_handled = true;

                break;
            }
        }
            
        if (!pipe_handled) {
            //fork process
            pid_t pid = fork();
            //check pids
            if (pid == 0) {  //check if child
                //check where output
                if (output_file_name != NULL) {
                    int fd = open(output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { //check for failure
                        perror("open file failed");
                        exit(1);
                    } else {
                        //redirect file
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                    }
                }

                if (input_file_name != NULL) {
                    int fd = open(input_file_name, O_RDONLY);
                    if (fd < 0) { //check for failure
                        perror("open file failed");
                        exit(1);
                    } else {
                        //redirect file
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                }

                execvp(args[0], args);
                //if execvp fails - print error and exit
                perror("execvp failed");
                exit(1);
            } else if (pid > 0) { //check if parent
                waitpid(pid, NULL, 0);
            } else { //otherwise error
                perror("fork failed");
            }
        }


    }

    return 0;
}