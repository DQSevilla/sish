#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int f_tracing_mode;

int main(int, char *[]);
static char *prompt(char *, size_t);
static void execute(char *);
static void interpret(void);

static void
execute(char *command) {

    // TODO: call builtin or fork(2) and exec(3) command
    // to avoid unused variable warnings:
    (void)printf("executing %s\n", command);
}

static char *
prompt(char *buffer, size_t buffer_size) {

    (void)printf("%s$ ", getprogname());
    return fgets(buffer, buffer_size, stdin);
}

static void
interpret(void) {
    char input[BUFSIZ];

    while (prompt(input, sizeof(input)) != NULL) {
        input[strlen(input) - 1] = '\0'; // overwrite newline character
    }
}

int
main(int argc, char *argv[]) {
    char *command;
    char opt;

    setprogname(argv[0]);
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
}
