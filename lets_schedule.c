#include <stdio.h>

int main (int ac, char **av) {
    printf("Let's schedule started ...\n");
    for (int i = 1; i < ac; i++) {
        printf("arg %d: %s\n", i, av[i]);
    }
    return 0;
}