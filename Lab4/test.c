#include <stdio.h>
#include <stdlib.h>
#include "tpool.h"
#include <unistd.h>
#include <time.h>

void test_function(int task);

int value = 0;

int main(int argc, char** argv) {

    if(tpool_init(test_function)) {
        perror("(main) tpool_init(): Failed to create thread pool.");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 100; i++) {
        if(i % 5 == 0)
            sleep(3);

        printf("(main): Adding %d\n", i);
        if(tpool_add_task(i)) {
            perror("(main) tpool_add_task(): Failed to add a task to the thread pool.");
        }
    }

    return EXIT_SUCCESS;

}

void test_function(int task) {

    printf("Task %d: Getting %d\n", task, value);
    value++;
    sleep(13000ULL * (int)sysconf(_SC_NPROCESSORS_ONLN) / 4 * rand() / RAND_MAX);
}
