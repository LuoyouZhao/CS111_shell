#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define MAX_PROGRAMS 10

void get_input(char* input) {
    printf("jsh$ ");
    fgets(input, MAX_INPUT_SIZE, stdin);
    input[strcspn(input, "\n")] = 0; // Remove trailing newline character
}

void trim_space(char* str) {
    char* end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    // If all spaces
    if(*str == 0)
        return;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';
}

void parse_input(char* input, char** programs, int* program_count) {
    char* token = strtok(input, "|");
    while (token != NULL && *program_count < MAX_PROGRAMS) {
        trim_space(token);  // Remove spaces around the program
        programs[(*program_count)++] = token;
        token = strtok(NULL, "|");
    }
}

void execute_program(char* program) {
    char* args[MAX_ARGS];
    char* token = strtok(program, " ");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL; // Terminate the args array
    if (execvp(args[0], args) < 0) { // Check if execvp fails
        fprintf(stderr, "jsh error: Command not found: %s\n", args[0]);
        exit(127); // Exit with status 127
    }
}


int main() {
    char input[MAX_INPUT_SIZE];
    char* programs[MAX_PROGRAMS];
    int program_count;
    pid_t pids[MAX_PROGRAMS];
    int status;
    int pipe_fds[2 * (MAX_PROGRAMS - 1)];

    while (1) {
        program_count = 0;
        get_input(input);

        if (strcmp(input, "exit") == 0) {
            printf("Exiting jsh.\n");
            break;
        }

        parse_input(input, programs, &program_count); // Split and trim input into separate programs

        // create pipe
        for (int i = 0; i < program_count - 1; i++) {
            if (pipe(pipe_fds + i * 2) < 0) {
                perror("Couldn't Pipe");
                exit(EXIT_FAILURE);
            }
        }

        for (int i = 0; i < program_count; i++) {
            pids[i] = fork();
            if (pids[i] == -1) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            } else if (pids[i] == 0) {
                // child process
                if (i > 0) {
                    dup2(pipe_fds[(i - 1) * 2], 0); // set stdin as the last stdout
                }
                if (i < program_count - 1) {
                    dup2(pipe_fds[i * 2 + 1], 1); // set stdout as the next stdin
                }
                // close pipe
                for (int j = 0; j < 2 * (program_count - 1); j++) {
                    close(pipe_fds[j]);
                }
                execute_program(programs[i]);
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
        }

        // close all pipes
        for (int i = 0; i < 2 * (program_count - 1); i++) {
            close(pipe_fds[i]);
        }

        // Wait for all child processes to finish
        for (int i = 0; i < program_count; i++) {
            waitpid(pids[i], &status, 0);
            if (i == program_count - 1) { // Print the last one's status
                printf("jsh status: %d\n", WEXITSTATUS(status));
            }
        }
    }

    return EXIT_SUCCESS;
}