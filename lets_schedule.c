#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>

typedef struct {
    char *algorithm;
    int priority;
    int cpu;
    int io;
} ThreadConfig;

ThreadConfig* extract_ThreadConfig(char *input) {
    static regex_t regex;
    static bool regex_compiled = false;
    regmatch_t matches[5]; // We expect 4 matches, but array also includes the entire matched string

    if (!regex_compiled) {
        if (regcomp(&regex, "([A-Z]+)/([0-9]+)/([0-9]+)cpu/([0-9]+)io", REG_EXTENDED) != 0) {
            printf("Could not compile regex\n");
            return NULL;
        }
        regex_compiled = true;
    }
    ThreadConfig *result = malloc(sizeof(ThreadConfig));
    if (regexec(&regex, input, 5, matches, 0) == 0) {
        // Extract the matches
        for (int i = 1; i < 5; i++) {
            char *match = strndup(input + matches[i].rm_so, matches[i].rm_eo - matches[i].rm_so);
            if (i == 1) {
                result->algorithm = match;
            } else if (i == 2) {
                result->priority = atoi(match);
            } else if (i == 3) {
                result->cpu = atoi(match);
            } else if (i == 4) {
                result->io = atoi(match);
            }
            if (i != 1) free(match); // Free the allocated memory for integers
        }
    } else {
        free(result);
        result = NULL;
    }
    return result;
}

int main (int ac, char **av) {
    printf("Let's schedule started ...\n");
    for (int i = 1; i < ac; i++) {
        ThreadConfig *e = extract_ThreadConfig(av[i]);
        if (e != NULL) {
            printf("Algorithm: %s, Priority: %d, CPU: %d, IO: %d\n", e->algorithm, e->priority, e->cpu, e->io);
            free(e->algorithm); // Free the allocated memory for string
            free(e); // Free the allocated memory for struct
        }
    }
    return 0;
}