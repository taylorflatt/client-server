#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "tpool.h"
#include "DTRACE.h"

/* Preprocessor constants. */
#define TASKS_PER_THREAD 1      /* Set higher for a real application, 1 for testing. */

/* Custom Types. */
typedef struct queue {
    int *buffer;
    int size;
    int count;
    int head;
    int tail;
    int ntasks;
    int nfree;
    pthread_mutex_t mtx_queue;
    pthread_mutex_t mtx_tasks;
    pthread_mutex_t mtx_free;
    pthread_cond_t cond_tasks;
    pthread_cond_t cond_free;
} queue_t;

typedef struct tpool {
    queue_t* queue;
    int nthreads;
    void(*subroutine)(int);
} tpool_t;

/* Global Variables. */
static tpool_t tpool;
static queue_t queue;

/* Prototypes. */
static int queue_init(int size);
static int thread_init();
static void* thread_loop(void* thread);
static int dequeue();
static int enqueue(int task);

/** Handles creating the threadpool and assigning a single thread to each code.
 * 
 * task: A pointer corresponding to a void function which the thread pool will execute.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int tpool_init(void (*task)(int)) {

    tpool.nthreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    int size = tpool.nthreads * TASKS_PER_THREAD;

    if(queue_init(size) != 0) {
        perror("(tpool_init) queue_init(): Failed to create the task queue.");
        return -1;
    }

    tpool.subroutine = task;
    tpool.queue = &queue;

    /* Create the threads for the thread pool. */
    for(int i = 0; i < tpool.nthreads; i++) {
        if (thread_init()) {
            perror("(tpool_init) thread_init(): Error creating thread -- aborting.");
            return -1;
        }
  
        DTRACE("Created thread %d\n", i);
    }

    return 0;
}

/** Handles creating the unbounded queue and initializing the locks.
 * 
 * size: The initial length of the queue.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int queue_init(int size) {

    if(size < 1) {
        perror("(queue_init) size: Invalid size less than 1 found.");
        return -1;
    }

    if ((queue.buffer =  malloc(size * sizeof(int))) == NULL) {
        perror("Failed to malloc memory for tpool queue");
        return -1; 
    }

    /* Setup the queue. */
    queue.size = size;
    queue.count = 0;
    queue.head = 0;
    queue.tail = 0;
    queue.ntasks = 0;
    queue.nfree = size;

    /* Initialize the mutexes for the queue. */
    if (pthread_mutex_init(&queue.mtx_queue, NULL) != 0 ||
        pthread_mutex_init(&queue.mtx_tasks, NULL) != 0 ||
        pthread_mutex_init(&queue.mtx_free, NULL) != 0) {
            perror("(queue_init) pthread_mutex_init: Failed to create thread pool mutexes.");
            return -1; 
    }

    /* Initialize the condition variables for the queue. */
    if (pthread_cond_init(&queue.cond_tasks, NULL) != 0 || pthread_cond_init(&queue.cond_free, NULL) != 0) {
        perror("(queue_init) pthread_cond_init: Failed to create the thread pool condition variables.");
        return -1; 
    }

    return 0;

}

/** Handles creating a thread and assigning it a friendly name.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int thread_init() {

    pthread_t threadid;

    /* Create the threads and detach them from the process. */
    if(pthread_create(&threadid, NULL, thread_loop, NULL) != 0) {
        perror("(thread_init) pthread_create: Failed to create worker thread.");
        return -1;
    }

    if(pthread_detach(threadid) != 0) {
        perror("(thread_init) pthread_detach: Failed to detach worker thread.");
        return -1;
    }

    return 0;
  }

/** A task consumer which removes tasks off the queue (if any) and performs the task.
 * 
 * thr: A thread which will perform a task. (Ignored)
 * 
 * Returns: None.
*/
static void* thread_loop(void *thr) {

    /* If the queue is empty, wait. Then run any new jobs. */
    while(1) {
        pthread_mutex_lock(&queue.mtx_tasks);

        /* Wait for a task to come into the queue. */
        while(queue.ntasks == 0) {
            pthread_cond_wait(&queue.cond_tasks, &queue.mtx_tasks);
        }

        /* Reduce the number of tasks on the queue by 1. */
        queue.ntasks--;
        pthread_mutex_unlock(&queue.mtx_tasks);

        /* Dequeue the task for processing. */
        int task = dequeue();

        /* Update the free slot count and signal that a position is available. */
        pthread_mutex_lock(&queue.mtx_free);
        queue.nfree++;
        pthread_mutex_unlock(&queue.mtx_free);

        pthread_cond_signal(&queue.cond_free);

        /* Process the task. */
        if(task != -1) {
            tpool.subroutine(task);
        }
    }

    return NULL;
}


/** Removes a task from the queue.
 * 
 * Returns: An integer representing the task to be completed.
*/
static int dequeue() {

    int task;
    
    pthread_mutex_lock(&queue.mtx_queue);

    if(queue.count > 0) {
        /* Grab the head of the queue for processing. */
        task = queue.buffer[queue.head];

        /* Move the head down. */
        queue.head = (queue.head + 1) % queue.size;
        queue.count--;
    }

    pthread_mutex_unlock(&queue.mtx_queue);

    return task;
}

/** Public interface for adding a task to a pool.
 * 
 * task: The particular task which is to be added.
 * 
 * Returns: An integer representing the task to be completed.
*/
int tpool_add_task(int task) {

    //Wait for free slot in tasks queue:
    pthread_mutex_lock(&queue.mtx_free);

    while (queue.nfree == 0) {
        pthread_cond_wait(&queue.cond_free, &queue.mtx_free);
    }

    //Update free count:
    queue.nfree--;
    pthread_mutex_unlock(&queue.mtx_free);

    //Enqueue newtask to tasks queue:
    if (enqueue(task) == -1) {
        perror("(tpool_add_task) enqueue(): Failed adding a task to the thread pool.");
        return -1;
    }

    //Update tasks count:
    pthread_mutex_lock(&queue.mtx_tasks);
    queue.ntasks++;
    pthread_mutex_unlock(&queue.mtx_tasks);

    //Signal that a task is avail:
    pthread_cond_signal(&queue.cond_tasks);

    return 0;
}

/** Adds a task to the queue. Resizes the queue if necessary. Signals that there is a new job.
 * 
 * task: The particular task which is to be added.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int enqueue(int task) {

    pthread_mutex_lock(&queue.mtx_queue);

    /* The queue is full - error in a bounded context. Should never call enqueue if full. */
    if(queue.count == queue.size) {
        pthread_mutex_unlock(&queue.mtx_queue);
        perror("(enqueue) tpool_size: The queue is bounded and enqueue was called on a full queue improperly.");
        return -1;
    }

    /* Add the task to the queue and move the tail to the new task. */
    queue.buffer[queue.tail] = task;
    queue.tail = (queue.tail + 1) % queue.size;
    queue.count++;

    pthread_mutex_unlock(&queue.mtx_queue);

    return 0;
}

