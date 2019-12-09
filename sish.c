#ifdef __linux__
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
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

#define TRUE 1
#define FALSE 0

int f_tracing_mode;
int retcode;
int is_executing; // whether sish is executing a command

static void print_prompt(void);
static void handle_sigint(int);
static char *prompt(char *, size_t);
static void run_command(char **, size_t, int, int);
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

    if (!is_executing) {
        print_prompt();
        if (fflush(stdout) == EOF) {
            err(EXIT_FAILURE, "fflush");
        }
    }
}

/* precondition: cmdvect has len strings and is NULL terminated */
static void
run_command(char **cmdvect, size_t len, int outfd, int infd) {
    size_t size;
    int wstatus;

    is_executing = TRUE; /* toggle signal handler behavior */
    if (strncmp(cmdvect[0], "exit", strlen(cmdvect[0])) == 0) {
        exit(retcode);
    }

    switch (fork()) {
    case -1:
        err(EXIT_FAILURE, "fork");
    case 0:
        if (outfd != -1 && dup2(outfd, STDOUT_FILENO) != STDOUT_FILENO) {
            err(EXIT_FAILURE, "could not redirect standard output: dup2");
        }
        if (infd != -1 && dup2(infd, STDIN_FILENO) != STDIN_FILENO) {
            err(EXIT_FAILURE, "could not redirect standard input: dup2");
        }

        size = strlen(cmdvect[0]);
        if (strncmp(cmdvect[0], "cd", size) == 0) {
            exit(cd(len, cmdvect));
        } else if (strncmp(cmdvect[0], "echo", size) == 0) {
            exit(echo(len, cmdvect, retcode));
        }

        (void)execvp(cmdvect[0], cmdvect);
        if (errno == ENOENT) {
            (void)fprintf(stderr, "%s: command not found\n", cmdvect[0]);
            exit(ERR_NOT_EXECUTED);
        } else {
            err(ERR_NOT_EXECUTED, "execvp");
        }
    default:
        (void)close(outfd);
        (void)close(infd);
        for (;;) {
            if (wait(&wstatus) == -1) {
                err(EXIT_FAILURE, "wait");
            }
            if (WIFSIGNALED(wstatus)) {
                retcode = ERR_NOT_EXECUTED;
                break;
            }
            if (WIFEXITED(wstatus)) {
                retcode = WEXITSTATUS(wstatus);
                break;
            }
        }
        is_executing = FALSE;
    }
}

static void
execute(char *command) {
    char *tokens[MAX_TOKENS], *cmd[MAX_TOKENS];
    char *token, *path;
    size_t i, len, curr, toklen;
    int outfd, infd, open_flags;

    if (f_tracing_mode) {
        (void)fprintf(stderr, "+");
    }
    token = strtok(command, COMMAND_DELIMS);
    for (len = 0; token != NULL; ++len) {
        if (f_tracing_mode) {
            (void)fprintf(stderr, " %s", token);
        }
        tokens[len] = token;
        token = strtok(NULL, COMMAND_DELIMS);
    }
    if (f_tracing_mode) {
        (void)fprintf(stderr, "\n");
    }

    outfd = infd = -1;
    for (i = 0, curr = 0; i < len; ++i) {
        token = tokens[i];
        toklen = strlen(token);

        if (strnstr(token, ">>>", toklen) != NULL ||
            strnstr(token, "<<", toklen) != NULL) {
            (void)fprintf(stderr, "parse error: invalid combination of "
                "redirection operators\n");
            return;
        }

        path = NULL;
        if (toklen > 2 && strncmp(token, ">>", 2) == 0) {
            path = token + 2;
            open_flags = O_WRONLY | O_CREAT | O_APPEND;
        } else if (strncmp(token, ">", 1) == 0) {
            path = token + 1;
            open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        } else if (strncmp(token, "<", 1) == 0) {
            (void)close(infd);
            if ((infd = open(token + 1, O_RDONLY)) == -1) {
                perror("open");
                return;
            }
            continue;
        }

        if (path != NULL) {
            (void)close(outfd);
            if ((outfd = open(path, open_flags,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
                perror("open");
                return;
            }
            continue;
        }

        cmd[curr++] = token;
    }

    if (curr == 0) {
        return;
    }

    len = curr;
    cmd[len] = NULL;
    run_command(cmd, len, outfd, infd);
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
    char *command, *path;
    char opt;

    setprogname(argv[0]);

    if ((path = realpath(getprogname(), NULL)) == NULL) {
        err(EXIT_FAILURE, "realpath");
    }

    if (setenv("SHELL", path, TRUE) == -1) {
        err(EXIT_FAILURE, "setenv");
    }

    free(path);

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
        return retcode;
    }

    interpret();
    return retcode;
}
