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
  char id;
  pthread_t pthread_id;
} thread;

typedef struct task_queue {
  pthread_mutex_t lock;  // lock for read-write actions on queue
  semaphore* has_task; // condition for presence of tasks on queue
  size_t len;
  int head;
  int tail;
  int* buffer;
} task_queue;

typedef struct tpool {
  thread** threads;
  int num_threads;
  void (*subroutine)(int);
  task_queue* queue;
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
    perror("Failed to create the task queue.");
    return -1;
  }
}

  static int queue_init(int len) {
    size_t init_len = len;
    tpool.queue = &queue;
    queue.buffer = (int *) malloc(init_len * sizeof(int));
  }
}