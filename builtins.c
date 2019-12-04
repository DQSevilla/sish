#include <sys/types.h>

#include<pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "builtins.h"

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

// precondition: argc > 0
int
echo(size_t argc, char **argv) {
    size_t i;

    if (argc > 1) {
        (void)printf("%s", argv[1]);
        for (i = 2; i < argc; ++i) {
            (void)printf(" %s", argv[i]);
        }
    }

    (void)printf("\n");
    return EXIT_SUCCESS;
}
