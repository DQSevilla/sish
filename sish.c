#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <err.h>
#include <stdio.h>
#include <unistd.h>

int f_tracing_mode;

int main(int, char *[]);

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
        /* to avoid warnings about not using command */
        (void)printf("executing %s\n", command);
    }
}
