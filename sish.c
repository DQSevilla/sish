#ifdef __linux__
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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

struct command {
    char *cmd[MAX_TOKENS];
    size_t len;
    int infd;
    int outfd;
};

int f_tracing_mode;
int retcode;
int is_executing; // whether sish is executing a command

static void print_prompt(void);
static void handle_sigint(int);
static char *prompt(char *, size_t);
static void run_command(struct command *);
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

static void
free_command(struct command *cmd) {

    (void)close(cmd->infd);
    (void)close(cmd->outfd);
    free(cmd);
}

/* precondition: cmd is initialized and cmd->cmd is NULL terminated */
static void
run_command(struct command *cmd) {
    size_t size;
    int wstatus;

    is_executing = TRUE; // toggles signal handler behavior
    if (strncmp(cmd->cmd[0], "exit", strlen(cmd->cmd[0])) == 0) {
        if (cmd->len > 1) {
            retcode = EXIT_FAILURE;
            (void)fprintf(stderr, "exit: unknown paramters\n");
        }
        free_command(cmd);
        exit(retcode);
    }

    switch (fork()) {
    case -1:
        err(EXIT_FAILURE, "fork");
    case 0:
        if (cmd->outfd != -1 &&
            dup2(cmd->outfd, STDOUT_FILENO) != STDOUT_FILENO) {
            err(EXIT_FAILURE, "could not redirect stdout: dup2");
        }
        if (cmd->infd != -1 &&
            dup2(cmd->infd, STDIN_FILENO) != STDIN_FILENO) {
            err(EXIT_FAILURE, "could not redirect stdin: dup2");
        }

        size = strlen(cmd->cmd[0]);
        if (strncmp(cmd->cmd[0], "cd", size) == 0) {
            exit(cd(cmd->len, cmd->cmd));
        } else if (strncmp(cmd->cmd[0], "echo", size) == 0) {
            exit(echo(cmd->len, cmd->cmd, retcode));
        }

        (void)execvp(cmd->cmd[0], cmd->cmd);
        free_command(cmd);
        if (errno == ENOENT || errno == EBADF) {
            errx(ERR_NOT_EXECUTED, "%s: command not found", cmd->cmd[0]);
        } else {
            err(ERR_NOT_EXECUTED, "execvp");
        }
    default:
        (void)close(cmd->outfd);
        (void)close(cmd->infd);
        for (;;) {
            if (wait(&wstatus) == -1) {
                free(cmd);
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

static struct command *
parse_command(char **tokens, size_t len) {
    struct command *cmd;
    char *token, *path;
    size_t i, curr, toklen;
    int open_flags;

    if ((cmd = (struct command *) malloc(sizeof(struct command))) == NULL) {
        perror("parsing command: malloc");
        return NULL;
    }

    cmd->infd = cmd->outfd = -1;
    for (i = 0, curr = 0; i < len; ++i) {
        token = tokens[i];
        toklen = strlen(token);

        if (strnstr(token, ">>>", toklen) != NULL ||
            strnstr(token, "<<", toklen) != NULL) {
            (void)fprintf(stderr, "parse error: invalid combination of "
                "redirection operators");
            return NULL;
        }

        path = NULL;
        if (toklen > 2 && strncmp(token, ">>", 2) == 0) {
            path = token + 2;
            open_flags = O_WRONLY | O_CREAT | O_APPEND;
        } else if (strncmp(token, ">", 1) == 0) {
            path = token + 1;
            open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        } else if (strncmp(token, "<", 1) == 0) {
            (void)close(cmd->infd);
            if (toklen > 2 && strncmp(token, "<>", 2) == 0) {
                token++;
            }
            if ((cmd->infd = open(token + 1, O_RDONLY)) == -1) {
                perror("parsing command: open");
                return NULL;
            }
            continue;
        }

        if (path != NULL) {
            (void)close(cmd->outfd);
            if ((cmd->outfd = open(path, open_flags,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
                perror("parsing command: open");
                return NULL;
            }
            continue;
        }
        cmd->cmd[curr++] = token;
    }

    if (curr == 0) {
        return NULL;
    }
    cmd->cmd[curr] = NULL; // for execvp
    cmd->len = curr;
    return cmd;
}

static void
execute(char *command) {
    struct command *cmd;
    char *tokens[MAX_TOKENS];
    char *token;
    size_t len;

    if ((token = strtok(command, COMMAND_DELIMS)) == NULL) {
        return;
    }

    if (f_tracing_mode) {
        (void)fprintf(stderr, "+");
    }

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

    if ((cmd = parse_command(tokens, len)) == NULL) {
        retcode = EXIT_FAILURE;
        return;
    }

    run_command(cmd);
    free(cmd);
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
    char progpath[PATH_MAX + 1];
    char *command;
    char opt;

    setprogname(argv[0]);

    if (realpath(getprogname(), progpath) == NULL) {
        err(EXIT_FAILURE, "realpath");
    }

    if (setenv("SHELL", progpath, TRUE) == -1) {
        err(EXIT_FAILURE, "setenv");
    }

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

    if (command == NULL) {
        interpret();
    } else {
        execute(command);
    }

    return retcode;
}
