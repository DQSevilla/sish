#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"

#define UNUSED(x) (void)(x)
#define MAX_TOKENS 256 // TODO: research system limits
#define COMMAND_DELIMS " \t"

int f_tracing_mode;
int retcode;

static void print_prompt(void);
static void handle_sigint(int);
static char *prompt(char *, size_t);
static void execute(char *);
static void interpret(void);
int main(int, char *[]);

static void
print_prompt(void) {

    (void)printf("%s$ ", getprogname());
}

static void
handle_sigint(int signo) {

    UNUSED(signo); // ignore unused parameter warning

    (void)printf("\n");
    print_prompt();
    if (fflush(stdout) == EOF) {
        err(EXIT_FAILURE, "fflush");
    }
    // TODO: properly set return code for $?. (130 on most shells)
}

static void
execute(char *command) {
    char *tokens[MAX_TOKENS];
    char *token;
    size_t len, i, size;

    token = strtok(command, COMMAND_DELIMS);
    for (len = 0; token != NULL; ++len) {
        tokens[len] = token;
        token = strtok(NULL, COMMAND_DELIMS);
    }

    if (len == 0) {
        return;
    }

    if (f_tracing_mode) {
        (void)fprintf(stderr, "+");
        for (i = 0; i < len; ++i) {
            (void)fprintf(stderr, " %s", tokens[i]);
        }
        (void)fprintf(stderr, "\n");
    }

    size = strlen(tokens[0]);

    if (strncmp(tokens[0], "cd", size) == 0) {
        retcode = cd(len, tokens);
    } else if (strncmp(tokens[0], "echo", size) == 0) {
        retcode = echo(len, tokens);
    } else if (strncmp(tokens[0], "exit", size) == 0) {
        exit(retcode);
    } else {
        // TODO: call builtin or fork(2) and exec(3) command
    }
}

static char *
prompt(char *buffer, size_t buffer_size) {

    print_prompt();
    return fgets(buffer, buffer_size, stdin);
}

static void
interpret(void) {
    char input[BUFSIZ];

    while (prompt(input, sizeof(input)) != NULL) {
        input[strlen(input) - 1] = '\0'; // overwrite newline character
        execute(input);
    }
}

int
main(int argc, char *argv[]) {
    char *command;
    char opt;

    setprogname(argv[0]);

    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        err(EXIT_FAILURE, "signal error");
    }

    command = NULL;
    while ((opt = getopt(argc, argv, "xc:")) != -1) {
        switch (opt) {
        case 'x':
            f_tracing_mode = 1;
            break;
        case 'c':
            command = optarg;
            break;
        default:
            (void)fprintf(stderr, "usage: %s [-x] [-c command]\n",
                getprogname());
            exit(EXIT_FAILURE);
        }
    }

    if (command != NULL) {
        execute(command);
        return EXIT_SUCCESS;
    }

    interpret();
    return retcode;
}
