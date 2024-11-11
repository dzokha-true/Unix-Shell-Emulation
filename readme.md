# My Shell - Custom Command Line Interface

## Introduction
Welcome to **Myshell**, a custom Unix-like shell implementation written in C. This shell provides support for executing basic commands, handling background processes, input and output redirection, and piping between commands. The shell mimics typical command-line behavior while allowing users to run multiple commands in sequence or concurrently.

## Features
- **Basic Command Execution:** Executes standard Unix commands using `execvp()`.
- **Background Process Support:** Allows commands to be run in the background using the `&` symbol.
- **Input and Output Redirection:** Supports redirecting input from a file using `<` and redirecting output to a file using `>`.
- **Pipelining:** Enables piping commands using `|`, allowing the output of one command to be passed as input to another.
- **Signal Handling:** Prevents zombie processes by handling `SIGCHLD` signals.
- **Graceful Exit:** Handles EOF (Ctrl+D) input and exits the shell gracefully.

## Code Structure
The shell is organized into the following main components:
1. **Signal Handling (`sigchld_handler`, `childTerminator`):** Manages termination of child processes to prevent zombie processes.
2. **Command Parsing (`parse_commands`):** Parses the user input into individual commands and sets flags for redirection and background execution.
3. **Command Execution (`execute`):** Executes the parsed commands, handling piping, input/output redirection, and background processes.
4. **Main Loop (`main`):** Continuously prompts the user for input, parses the commands, and executes them.

## Signal Handling
The shell uses the `SIGCHLD` signal to reap terminated child processes. This prevents the accumulation of zombie processes:

```c
void sigchld_handler(int proc_id) {
    waitpid(proc_id, NULL, 0);
}

struct sigaction childTerminator = {
    .sa_handler = sigchld_handler,
    .sa_flags = SA_NOCLDWAIT,
};
```

The `sigchld_handler` function calls `waitpid()` to clean up child processes as they terminate.

## Command Parsing
The `parse_commands()` function tokenizes the user input, splitting it into individual commands and checking for special symbols (`&`, `<`, `>`, `|`):

```c
char** parse_commands(char *user_input, bool *background_processes, bool *input_from_file, bool *output_to_file, int *command_count) {
    *input_from_file = false;
    *output_to_file = false;
    *background_processes = false;
    *command_count = 0;

    // Check for special characters
    if (strpbrk(user_input, "&")) *background_processes = true;
    if (strpbrk(user_input, "<")) *input_from_file = true;
    if (strpbrk(user_input, ">")) *output_to_file = true;

    // Split commands by pipe (|)
    char **commands_from_shell = (char **)malloc(100 * sizeof(char *));
    char *token = strtok(user_input, "|");

    while (token) {
        commands_from_shell[*command_count] = strdup(token);
        (*command_count)++;
        token = strtok(NULL, "|");
    }

    return commands_from_shell;
}
```

## Command Execution
The `execute()` function handles different scenarios for executing commands:
- Single command (with optional input/output redirection)
- First command in a pipeline
- Middle commands in a pipeline
- Last command in a pipeline

It uses `fork()` to create child processes and `execvp()` for command execution. Pipes are created for inter-process communication.

### Example: Executing a Single Command
```c
if (*command_count == 1) {
    pid_t proc_id = fork();

    if (proc_id == 0) { // Child process
        if (*input_from_file) dup2(read_from, 0);
        if (*output_to_file) dup2(write_to, 1);
        execvp(argument_list[0], argument_list);
        exit(errno);
    } else {
        waitpid(proc_id, NULL, 0);
    }
}
```

### Example: Handling Pipes
```c
if (i > 0 && i < (*command_count - 1)) { // Middle commands
    pid_t proc_id = fork();

    if (proc_id == 0) { // Child process
        dup2(pipes[i - 1][0], 0);
        dup2(pipes[i][1], 1);
        execvp(argument_list[0], argument_list);
        exit(errno);
    } else {
        close(pipes[i][1]);
        waitpid(proc_id, NULL, 0);
    }
}
```

## Main Function
The main function sets up the shell environment, initializes flags, and runs the main loop to receive and process user input:

```c
int main(int argc, char *argv[]) {
    sigaction(SIGCHLD, &childTerminator, NULL);

    bool *background_processes = malloc(sizeof(bool));
    bool *input_from_file = malloc(sizeof(bool));
    bool *output_to_file = malloc(sizeof(bool));
    int *command_count = malloc(sizeof(int));

    while (1) {
        printf("my_shell$ ");
        char user_input[512];
        fgets(user_input, 512, stdin);

        if (feof(stdin)) {
            printf("\n");
            _exit(0);
        }

        char **commands = parse_commands(user_input, background_processes, input_from_file, output_to_file, command_count);
        execute(commands, background_processes, input_from_file, output_to_file, command_count);
    }
}
```

## Memory Management
- The shell dynamically allocates memory for command strings and argument lists.
- After execution, allocated memory is freed to prevent memory leaks.

## Compilation
To compile the shell, use the following command:
```bash
gcc -o my_shell my_shell.c
```

## Usage
Launch the shell by running:
```bash
./my_shell
```
Additionally, you may launch the shell in the silent mode (it will not print out the my_shell$ prompt) 
```bash
./my_shell -n
```

### Example Commands:
1. Basic command:
   ```bash
   my_shell$ ls -l
   ```
2. Command with input redirection:
   ```bash
   my_shell$ sort < input.txt
   ```
3. Command with output redirection:
   ```bash
   my_shell$ echo "Hello, World!" > output.txt
   ```
4. Command with background execution:
   ```bash
   my_shell$ sleep 10 &
   ```
5. Pipelining commands:
   ```bash
   my_shell$ cat file.txt | grep "keyword" | sort
   ```

## Error Handling
- Displays error messages using `errno` if a system call fails (e.g., `fork()`, `execvp()`, `dup2()`).
- Exits gracefully if user input is invalid or if the shell encounters a critical error.

## Future Enhancements
- Implement support for additional shell features (e.g., command history, environment variables).
- Improve error handling for edge cases (e.g., malformed input, invalid file paths).
- Optimize memory usage by freeing allocated resources more effectively.

## Conclusion
This shell implementation provides a foundation for a Unix-like command-line interface with basic features. It can be further extended and optimized for more advanced usage. Enjoy exploring and customizing your own shell!