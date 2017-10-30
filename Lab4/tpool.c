#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "tpool.h"
#include "DTRACE.h"

/* Custom Types. */
typedef struct semaphore {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int flag;
} semaphore;

typedef struct thread {
    char id;                  /* Friendly ID. */
    pthread_t pthread_id;     /* Pointer to actual thread. */
} thread;

typedef struct task_queue {
    pthread_mutex_t mutex;    /* R/W Lock */
    semaphore* has_jobs;      /* Contains any tasks. */
    size_t len;               /* Number of jobs in queue. */
    int head;                 /* Front of the queue. */
    int tail;                 /* End of the queue. */
    int* buffer;              /* Buffer. */
} task_queue;

typedef struct tpool {
    thread** threads;         /* Pointer to the threads. */
    int num_threads;          /* Number of threads in the pool. */
    void (*subroutine)(int);  /* Function assigned. */
    task_queue* queue;        /* Thread pool queue. */
} tpool_t;

/* Global Variables. */
static tpool_t tpool;
static task_queue queue;

/* Prototypes. */

int tpool_add_task(int task)
static int queue_init(int len);

int tpool_init(void (*task)(int)) {

    int numProcessors = (int)sysconf(_SC_NPROCESSORS_ONLN);   
    tpool.num_threads = numProcessors;                        /* Create threads relative to num CPUs. */
    tpool.subroutine = task;                                  /* Function assigned to pool. */

    if(queue_init(numProcessors) != 0) {
        perror("(tpool_init) queue_init(): Failed to create the task queue.");
        return -1;
    }

    /* Create memory for the threads. */
    tpool.threads = (thread**) malloc(tpool.num_threads * sizeof(thread*));
    if (tpool.threads == NULL) {
        perror("(tpool_init) tpool_init(): Failed to malloc memory for thread.");
        return -1;
    }

    int i = 0;
    do {
      if (thread_init(&(tpool.threads[i]), i)) {
          fprintf(stderr, "(tpool_init) thread_init(): Error creating thread -- aborting\n");
          return -1;
      }

      DTRACE("Created thread %c\n", tpool.threads[i]->id);

    } while (++i < tpool.num_threads);


    return 0;
}

  static int queue_init(int len) {

    size_t init_len = len;
    tpool.queue = &queue;
    queue.buffer = (int *) malloc(init_len * sizeof(int));


    if (queue.buffer == NULL) {
        perror("queue_init(): Failed to allocate memory for queue buffer");
        return -1;
    }

    /* Create memory for the semaphore. */
    queue.has_task = (sem*) malloc(sizeof(sem));
    if (queue.has_task == NULL) {
        perror("queue_init(): Failed to allocate memory for sem");
        return -1;
    }

    /* Create an empty queue. */
    queue.len = init_len;
    queue.head = 0;
    queue.tail = 0;

    /* Initialize the locks and set waiting. */
    pthread_mutex_init(&queue.lock, NULL);
    pthread_mutex_init(&(queue.has_task->mutex), NULL);
    pthread_cond_init(&(queue.has_task->condition), NULL);
    sem->flag = 0;

    return 0;
}

static int thread_init(thread** threadptr, int ord) {

    *threadptr = (thread*) malloc(sizeof(thread));
    if (threadptr == NULL) {
        perror("(thread_init): Failed to allocate memory for thread");
        return -1;
    }

    /* Give them simple names A, B, C, ..., Z. */
    (*threadptr)->id = 'A' + ord;

    /* Create the threads and detach them from the process. */
    pthread_create(&((*threadptr)->pthread_id), NULL, thread_loop, (*threadptr));
    pthread_detach((*threadptr)->pthread_id);

    return 0;
  }