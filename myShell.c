#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

void sigchld_handler (int proc_id) { // reaping the exit code by calling wait on the processes
    waitpid(proc_id, NULL, 0);
}

struct sigaction childTerminator = { // signal handling for preventing zombie processes of children processes
        .sa_handler = sigchld_handler,
        .sa_sigaction = NULL,
        .sa_mask = {},
        .sa_flags = SA_NOCLDWAIT,
        .sa_restorer = NULL,
};


char** parse_commands(char *user_input, bool* background_processes, bool* input_from_file, bool* output_to_file, int* command_count ) {
    //the function that parses the commands from the command line, and sets the redirection and execution flags

    //initialization of the flags.
    *input_from_file = false;
    *output_to_file = false;
    *background_processes = false;
    *command_count = 0;

    if ( strpbrk(user_input, "&") != NULL ) { //check if background execution is enabled
        *background_processes = true;
    }

    if ( strpbrk(user_input, "<") != NULL ) { //check if input should be read from the file
        *input_from_file = true;
    }

    if ( strpbrk(user_input, ">") != NULL ) { //check if output redirection is in order
        *output_to_file = true;
    }


    if (user_input == NULL) { //If the user has entered nothing in the shell, exit.
        exit(0);
    }


    else {
        //creating an array of strings, where each element is the argument for the executed command.
        char **commands_from_shell = (char **)malloc(100 * sizeof(char*) );
        memset(commands_from_shell, 0, 100*sizeof(char*)); //Setting the memory to null

        int i = 0;
        char* token; //token is a collection of commands and their arguments delimited with pipe signs

        token = strtok(user_input, "|");
        while (token != NULL) {
            // allocating max 32 character string that holds a single commands with its arguments
            commands_from_shell[i] = (char*)malloc( (sizeof(char) * 32) + 1);

            while (*token == ' ') { //get rid of the leading whitespace in the token
                token++;
            }

            char * end = (token + strlen(token) - 1 );
            while (*end == ' ') { // get rid of the trailing whitespace
                end--;
            }
            *(end+1) = '\0';

            fflush(stdout);

            *command_count = *command_count + 1;
            strcpy(commands_from_shell[i], token);

            token = strtok(NULL, "|");
            i++;
        }
        return commands_from_shell;
    }
}


void execute( char **commands_from_shell, bool* background_processes, bool* input_from_file, bool* output_to_file, int* command_count ) {
    int i = 0;

    //allocating 2d array that holds pointers to pipes. e.g pipes[0] holds 1st pipe.
    int ** pipes = (int **)malloc(((*command_count-1)) * sizeof(int*) );

    for (i=0; i < *command_count; i++) { //allocating 2 integers for each pipe[i]
        pipes[i] = (int*)malloc(2*sizeof(int));
    }

    //initialization of the argument list for execvp() command
    char ** argument_list = (char **)malloc(100 * sizeof(char*) );

    //initialization of the strings for the names of the i/o files
    char infile[100];
    char outfile[100];

    //initialization of the file descriptor numbers
    int read_from;
    int write_to;

    if (*background_processes == true) {
        char *ptr = strpbrk(commands_from_shell[ (*command_count - 1) ], "&" );
        *ptr = '\0';
    }

    if (*output_to_file == true) {
        char *ptr = strpbrk(commands_from_shell[(*command_count - 1)], ">");    //to help with argument list creation, I will null terminate the first occurance of the redirection symbol
        *ptr = ' ';
        ptr++;
        int k = 0;
        while ((*ptr != '\0') ) {
            if (*ptr == ' ') {ptr++;}

            else {
                outfile[k] = *ptr;
                char next = *(ptr+1);
                if ( (next == ' ') || (next == '\0') ) {
                    *(ptr) = ' ';
                    ptr++;
                    k++;
                    break;
                }
                *ptr = ' ';
                ptr++;
                k++;
            }
        }
        outfile[k] = '\0';

        FILE* output_file = fopen(outfile, "w");
        if (output_file != NULL){
            write_to = fileno(output_file);
        } else {printf("ERROR: error code %d, %s\n", errno, strerror(errno));}
    }

    if (*input_from_file == true) {
        char *ptr = strpbrk(commands_from_shell[0], "<");       //to help with argument list creation, I will null terminate the first occurance of the redirection symbol
        *ptr = ' ';
        ptr++;
        int k = 0;
        while (*ptr != '\0') {
            if (*ptr == ' ') {ptr++;}

            else{
                infile[k] = *ptr;
                char next = *(ptr+1);
                if ( (next == ' ') || (next == '\0')) {
                    *ptr = ' ';
                    ptr++;
                    k++;
                    break;
                }
                *ptr = ' ';
                ptr++;
                k++;
            }
        }
        infile[k] = '\0';

        FILE* input_file = fopen(infile, "r");
        if (input_file != NULL){
            read_from = fileno(input_file);
        } else {printf("ERROR: error code %d, %s", errno, strerror(errno));}

    }

    for (i = 0; i < *command_count; i++) {
        // Clear argument list from previous values at the start of the iteration
        memset(argument_list, 0, 100 * sizeof(char*));

        // Tokenize the current command from shell input
        char* token = strtok(commands_from_shell[i], " ");

        // Create a pipe for inter-process communication
        if (pipe(pipes[i]) != 0) {
            printf("ERROR: error code %d, %s \n", errno, strerror(errno));
        }

        int j = 0;
        while (token != NULL) {
            argument_list[j] = (char*)malloc((sizeof(char*) * 16) + 1);
            strcpy(argument_list[j], token);
            token = strtok(NULL, " ");
            j++;
        }

        fflush(stdout);

        // Case 1: Single command (no piping)
        if (*command_count == 1) {
            pid_t proc_id = fork();

            if (proc_id == 0) { // Child process
                fflush(stdout);

                if (*input_from_file) {
                    if (dup2(read_from, 0) == -1) {
                        printf("ERROR: error code %d, %s", errno, strerror(errno));
                    }
                }

                if (*output_to_file) {
                    if (dup2(write_to, 1) == -1) {
                        printf("ERROR: error code %d, %s", errno, strerror(errno));
                    }
                }

                execvp(argument_list[0], argument_list);
                printf("ERROR: error code %d, %s \n", errno, strerror(errno));
                exit(errno);
            } else if (proc_id > 0) { // Parent process
                int status;

                if (*input_from_file) close(read_from);
                if (*output_to_file) close(write_to);

                if (!(*background_processes)) waitpid(proc_id, &status, 0);
            } else {
                printf("ERROR: error code %d, %s \n", errno, strerror(errno));
            }
        }

            // Case 2: First command in a chain of pipes
        else if (i == 0) {
            pid_t proc_id = fork();

            if (proc_id == 0) { // Child process
                fflush(stdout);

                if (*input_from_file) {
                    if (dup2(read_from, 0) == -1) {
                        printf("ERROR: error code %d, %s", errno, strerror(errno));
                    }
                } else {
                    close(pipes[0][0]);
                }

                dup2(pipes[0][1], 1);
                execvp(argument_list[0], argument_list);
                printf("ERROR: error code %d, %s \n", errno, strerror(errno));
                exit(errno);
            } else if (proc_id > 0) { // Parent process
                int status;

                if (!(*background_processes)) {
                    waitpid(proc_id, &status, 0);
                    close(pipes[0][1]);
                }

                if (*input_from_file) close(read_from);
            } else {
                printf("ERROR: error code %d, %s \n", errno, strerror(errno));
            }
        }

            // Case 3: Middle commands in a chain of pipes
        else if (i > 0 && i < (*command_count - 1)) {
            pid_t proc_id = fork();

            if (proc_id == 0) { // Child process
                dup2(pipes[i - 1][0], 0);
                dup2(pipes[i][1], 1);
                execvp(argument_list[0], argument_list);
                printf("ERROR: error code %d, %s \n", errno, strerror(errno));
                exit(0);
            } else if (proc_id > 0) { // Parent process
                close(pipes[i][1]);
                int status;

                if (!(*background_processes)) {
                    waitpid(-1, &status, 0);
                    close(pipes[i - 1][0]);
                    close(pipes[i][1]);
                }
            } else {
                printf("ERROR: failed to fork, exited with error code: %d \n", proc_id);
            }
        }

            // Case 4: Last command in a chain of pipes
        else {
            pid_t proc_id = fork();

            if (proc_id == 0) { // Child process
                dup2(pipes[i - 1][0], 0);

                if (*output_to_file) {
                    if (dup2(write_to, 1) == -1) {
                        printf("ERROR: error code %d, %s", errno, strerror(errno));
                    }
                }

                execvp(argument_list[0], argument_list);
                printf("ERROR: error code %d, %s \n", errno, strerror(errno));
                exit(0);
            } else if (proc_id > 0) { // Parent process
                int status;

                if (!(*background_processes)) {
                    waitpid(-1, &status, 0);
                    close(pipes[i - 1][1]);
                    close(pipes[i - 1][0]);
                }

                if (*output_to_file) close(write_to);
            } else {
                printf("ERROR: failed to fork, exited with error code: %d \n", proc_id);
            }
        }
    }

// Free allocated memory for pipes
    for (i = 0; i < (*command_count) - 1; i++) {
        free(pipes[i]);
    }

// Free allocated memory for commands and argument lists
    for (i = 0; i < 100; i++) {
        free(commands_from_shell[i]);
        free(argument_list[i]);
    }

}



int main(int argc, char *argv[]) {
    // Set up signal handler for terminating child processes
    sigaction(SIGCHLD, &childTerminator, NULL);

    // Flags to control shell behavior
    bool print_my_shell = false;

    // Allocate memory for control flags
    bool *background_processes = (bool *)malloc(sizeof(bool));
    *background_processes = false;

    bool *input_from_file = (bool *)malloc(sizeof(bool));
    *input_from_file = false;

    bool *output_to_file = (bool *)malloc(sizeof(bool));
    *output_to_file = false;

    // Allocate memory for command count
    int *command_count = (int *)malloc(sizeof(int));
    *command_count = 0;

    while (1) {
        // Check for '-n' flag in arguments to suppress shell prompt
        if (argc > 1 && strcmp(argv[1], "-n") == 0) {
            print_my_shell = false;
        } else {
            print_my_shell = true;
        }

        // Display shell prompt if enabled
        if (print_my_shell) {
            printf("my_shell$ ");
        }

        // Read user input from stdin
        char user_input[512];
        fgets(user_input, 512, stdin);

        // Handle EOF (Ctrl+D) gracefully
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            _exit(0);
        }

        // Remove the trailing newline character from user input
        char *new_line = strpbrk(user_input, "\n");
        *new_line = '\0';

        // Parse the commands from user input
        char **returned = parse_commands(user_input, background_processes, input_from_file, output_to_file, command_count);
        fflush(stdout);

        // Execute the parsed commands
        execute(returned, background_processes, input_from_file, output_to_file, command_count);
    }
}
