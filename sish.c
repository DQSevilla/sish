#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtins.h"

#define UNUSED(x) (void)(x)
#define ERR_NOT_EXECUTED 127
#define MAX_TOKENS 256 // TODO: research system limits
#define COMMAND_DELIMS " \t"

int f_tracing_mode;
int retcode;

static void print_prompt(void);
static void handle_sigint(int);
static char *prompt(char *, size_t);
static void run_command(char **, size_t);
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
}

// precondition: cmdvect has len strings and is NULL terminated
static void run_command(char **cmdvect, size_t len) {
    size_t size;
    int wstatus;

    size = strlen(cmdvect[0]);

    if (strncmp(cmdvect[0], "cd", size) == 0) {
        retcode = cd(len, cmdvect);
    } else if (strncmp(cmdvect[0], "echo", size) == 0) {
        retcode = echo(len, cmdvect, retcode);
    } else if (strncmp(cmdvect[0], "exit", size) == 0) {
        exit(retcode);
    } else {
        switch (fork()) {
        case -1:
            err(EXIT_FAILURE, "fork");
        case 0:
            (void)execvp(cmdvect[0], cmdvect);
            if (errno == ENOENT) {
                (void)fprintf(stderr, "%s: command not found\n", cmdvect[0]);
                exit(ERR_NOT_EXECUTED);
            } else {
                err(ERR_NOT_EXECUTED, "execvp");
            }
        default:
            do {
                if (wait(&wstatus) == -1) {
                    err(EXIT_FAILURE, "wait");
                }
            } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
            retcode = WEXITSTATUS(wstatus);
        }
    }
}

static void
execute(char *command) {
    char *tokens[MAX_TOKENS];
    char *token;
    size_t len, i;

    /* vectorize command string */
    token = strtok(command, COMMAND_DELIMS);
    for (len = 0; token != NULL; ++len) {
        tokens[len] = token;
        token = strtok(NULL, COMMAND_DELIMS);
    }
    tokens[len] = NULL; // execvp expects NULL terminated vector

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

    run_command(tokens, len);
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
