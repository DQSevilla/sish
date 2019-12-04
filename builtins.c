#include <sys/types.h>

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"

static void echo_handler(char *, int);

// precondtion: argc > 0
int
cd(size_t argc, char **argv) {
    struct passwd *pw;
    char *dir;

    switch (argc) {
    case 1:
        if ((pw = getpwuid(getuid())) == NULL) {
            perror("cd: getpwuid");
            return EXIT_FAILURE;
        }
        dir = pw->pw_dir;
        break;
    case 2:
        dir = argv[1];
        break;
    default:
        (void)fprintf(stderr, "cd: too many arguments\n");
        return EXIT_FAILURE;
    }
    
    if (chdir(dir) == -1) {
        perror("cd: chdir");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void
echo_handler(char *str, int retcode) {

    if (strncmp(str, "$?", strlen(str)) == 0) {
        (void)printf("%d", retcode);
    } else if (strncmp(str, "$$", strlen(str)) == 0) {
        (void)printf("%d", getpid());
    } else {
        (void)printf("%s", str);
    }
}

// precondition: argc > 0
int
echo(size_t argc, char **argv, int retcode) {
    size_t i;

    if (argc > 1) {
        echo_handler(argv[1], retcode);
        for (i = 2; i < argc; ++i) {
            (void)printf(" ");
            echo_handler(argv[i], retcode);
        }
    }

    (void)printf("\n");
    return EXIT_SUCCESS;
}
