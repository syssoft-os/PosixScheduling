#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

float cpu_burst(int n) {
    float result = 0.0;
    for (int i = 0; i < n; i++) {
        result += sqrt(i);
    }
    return result;
}

int calibrate_cpu_burst() {
    int low = 0, high = 1e6;
    double elapsed;
    clock_t start, end;

    while (high - low > 100) { // Tolerance of 100 iterations
        int mid = (low + high) / 2;
        start = clock();
        cpu_burst(mid);
        end = clock();
        elapsed = ((double) (end - start)) / CLOCKS_PER_SEC * 1000; // Convert to milliseconds

        if (elapsed < 1.0) {
            low = mid;
        } else {
            high = mid;
        }
    }

    return (low + high) / 2;
}

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

typedef struct {
    ThreadConfig *config;
    int calibrate_1ms;
    int simulation_seconds;
} ThreadArgs;

void* thread_func(void *arg) {
    ThreadArgs *args = (ThreadArgs*) arg;
    ThreadConfig *config = args->config;
    int calibrate_1ms = args->calibrate_1ms;
    int simulation_seconds = args->simulation_seconds;
    time_t start_time = time(NULL);
    while (1) {
        if (config->cpu > 0)
            cpu_burst(config->cpu * calibrate_1ms);
        if (config->io > 0)
            usleep(config->io * 1000); // usleep takes microseconds
        time_t current_time = time(NULL);
        double elapsed_time = difftime(current_time, start_time);

        if (elapsed_time >= simulation_seconds) {
            break;
        }
    }

    return NULL;
}

pthread_t create_thread(int simulation_seconds, ThreadConfig *config, int calibrate_1ms) {
    pthread_t thread_id;
    ThreadArgs *args = malloc(sizeof(ThreadArgs));
    args->config = config;
    args->calibrate_1ms = calibrate_1ms;
    args->simulation_seconds = simulation_seconds;

    pthread_create(&thread_id, NULL, thread_func, args);
    // Set scheduling policy and priority
    struct sched_param param;
    param.sched_priority = config->priority;
    int policy = (strcmp(config->algorithm, "RR") == 0) ? SCHED_RR : SCHED_FIFO;
    int ret = pthread_setschedparam(thread_id, policy, &param);
    if (ret == EPERM) {
        fprintf(stderr, "Failed to set thread scheduling policy and priority. Are you root?\n");
        fflush(stderr);
        exit(-1);
    } else if (ret != 0) {
        fprintf(stderr, "Failed to set thread scheduling policy and priority. Error: %d\n", ret);
        fflush(stderr);
        exit(-1);
    }

    return thread_id;
}

int main (int ac, char **av) {
    if (ac < 3) {
        printf("Usage: %s <simulation_seconds> <thread_config> ...\n", av[0]);
        return -1;
    }
    printf("Let's schedule started ...\n");
    int simulation_seconds = atoi(av[1]);
    printf("Simulation will run for %d seconds\n", simulation_seconds);
    int calibrate_1ms = calibrate_cpu_burst();
    printf("Calibrating CPU burst of 1ms: %d\n", calibrate_1ms);
    int n_threads = ac - 2;
    printf("Number of threads: %d\n", n_threads);
    pthread_t *threads = malloc(sizeof(pthread_t) * n_threads);
    for (int i = 2; i < ac; i++) {
        ThreadConfig *e = extract_ThreadConfig(av[i]);
        if (e != NULL) {
            printf("Algorithm: %s, Priority: %d, CPU: %d, IO: %d\n", e->algorithm, e->priority, e->cpu, e->io);
            pthread_t thread_id = create_thread(simulation_seconds, e, calibrate_1ms);
            threads[i - 2] = thread_id;
            free(e->algorithm); // Free the allocated memory for string
            free(e); // Free the allocated memory for struct
        }
        else {
            printf("Invalid thread config: %s\n", av[i]);
            printf("Thread config must be in the format: <algorithm>/<priority>/<cpu>cpu/<io>io\n");
            printf("Example: FIFO/1/100cpu/1000io\n");
            return -1;
        }
    }
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}