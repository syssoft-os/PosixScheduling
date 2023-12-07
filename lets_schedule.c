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

typedef struct BurstData BurstData;

typedef struct {
    char *raw;
    char *algorithm;
    int priority;
    int cpu;
    int io;
    pthread_t thread;
    int calibrate_1ms;
    int simulation_seconds;
    BurstData *burst_data;

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
    result->raw = strdup(input);
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

struct BurstData {
    double cpu_burst_length;
    double io_burst_length;
    BurstData *next;
};

void* thread_func(void *arg) {
    ThreadConfig *args = (ThreadConfig*) arg;
    int calibrate_1ms = args->calibrate_1ms;
    int simulation_seconds = args->simulation_seconds;
    args->burst_data = NULL;
    time_t start_time = time(NULL);
    while (1) {
        BurstData *data = malloc(sizeof(BurstData));
        time_t burst_start, burst_end;
        burst_start = time(NULL);
        if (args->cpu > 0) {
            cpu_burst(args->cpu * calibrate_1ms);
        }
        burst_end = time(NULL);
        data->cpu_burst_length = difftime(burst_end, burst_start);
        burst_start = time(NULL);
        if (args->io > 0) {
            usleep(args->io * 1000); // usleep takes microseconds
        }
        burst_end = time(NULL);
        data->io_burst_length = difftime(burst_end, burst_start);
        printf("%f %f\n", data->cpu_burst_length, data->io_burst_length);
        data->next = args->burst_data;
        args->burst_data = data;

        time_t current_time = time(NULL);
        double elapsed_time = difftime(current_time, start_time);

        if (elapsed_time >= simulation_seconds) {
            break;
        }
    }

    return NULL;
}

void create_thread(ThreadConfig *config ) {
    pthread_create(&config->thread, NULL, thread_func, config);
    // Set scheduling policy and priority
    struct sched_param param;
    param.sched_priority = config->priority;
    int policy = (strcmp(config->algorithm, "RR") == 0) ? SCHED_RR : SCHED_FIFO;
    int ret = pthread_setschedparam(config->thread, policy, &param);
    if (ret == EPERM) {
        fprintf(stderr, "Failed to set thread scheduling policy and priority. Are you root?\n");
        fflush(stderr);
        exit(-1);
    } else if (ret != 0) {
        fprintf(stderr, "Failed to set thread scheduling policy and priority. Error: %d\n", ret);
        fflush(stderr);
        exit(-1);
    }
}

void print_burst_stats(ThreadConfig *config) {
    // Calculate mean and standard deviation
    BurstData *data = config->burst_data;
    double cpu_sum = 0, io_sum = 0;
    int count = 0;
    while (data != NULL) {
        cpu_sum += data->cpu_burst_length;
        io_sum += data->io_burst_length;
        count++;
        data = data->next;
    }
    double cpu_mean = cpu_sum / count;
    double io_mean = io_sum / count;

    double cpu_sq_diff_sum = 0, io_sq_diff_sum = 0;
    data = config->burst_data;
    while (data != NULL) {
        cpu_sq_diff_sum += pow(data->cpu_burst_length - cpu_mean, 2);
        io_sq_diff_sum += pow(data->io_burst_length - io_mean, 2);
        data = data->next;
    }
    double cpu_std_dev = sqrt(cpu_sq_diff_sum / count);
    double io_std_dev = sqrt(io_sq_diff_sum / count);

    printf("Thread %s: count = %d, CPU mean = %f, CPU std dev = %f, IO mean = %f, IO std dev = %f\n",
           config->raw, count, cpu_mean, cpu_std_dev, io_mean, io_std_dev);
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
    ThreadConfig **threads = malloc(sizeof(ThreadConfig *) * n_threads);
    for (int i = 2; i < ac; i++) {
        ThreadConfig *e = extract_ThreadConfig(av[i]);
        e->calibrate_1ms = calibrate_1ms;
        e->simulation_seconds = simulation_seconds;
        if (e != NULL) {
            printf("Algorithm: %s, Priority: %d, CPU: %d, IO: %d\n", e->algorithm, e->priority, e->cpu, e->io);
            create_thread(e);
            threads[i - 2] = e;
        }
        else {
            printf("Invalid thread config: %s\n", av[i]);
            printf("Thread config must be in the format: <algorithm>/<priority>/<cpu>cpu/<io>io\n");
            printf("Example: FIFO/1/100cpu/1000io\n");
            return -1;
        }
    }
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i]->thread, NULL);
    }
    for (int i = 0; i < n_threads; i++) {
        print_burst_stats(threads[i]);
    }
    return 0;
}