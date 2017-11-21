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
    pthread_mutex_t mutex;      /* Lock */
    pthread_cond_t condition;   /* Condition variable */
    int flag;                   /* Number of keys */
} semaphore;

typedef struct thread {
    char id;                  /* Friendly ID. */
    pthread_t pthread_id;     /* Pointer to actual thread. */
} thread;

typedef struct job_queue {
    pthread_mutex_t mutex;    /* R/W mutex */
    semaphore* has_jobs;      /* Contains any tasks. */
    size_t len;               /* Number of jobs in queue. */
    int head;                 /* Front of the queue. */
    int tail;                 /* End of the queue. */
    int* buffer;              /* Buffer. */
} job_queue;

typedef struct tpool {
    thread** threads;         /* Pointer to the threads. */
    int num_threads;          /* Number of threads in the pool. */
    void (*subroutine)(int);  /* Function assigned. */
    job_queue* queue;         /* Thread pool queue. */
} tpool_t;

/* Global Variables. */
static tpool_t tpool;
static job_queue queue;

/* Prototypes. */
static int queue_init();
static int thread_init(thread** threadpp, int ord);
static void* thread_loop(void* thread);
static void pool_wait(semaphore* sem);
static int dequeue(job_queue* q);
static int enqueue(job_queue* q, int task);
static int resize_queue(job_queue* q);



/** Handles creating the threadpool and assigning a single thread to each code.
 * 
 * task: A pointer corresponding to a void function which the thread pool will execute.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
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

    /* Create the threads for the thread pool. */
    for(int i = 0; i < tpool.num_threads; i++) {
        if (thread_init(&(tpool.threads[i]), i)) {
            fprintf(stderr, "(tpool_init) thread_init(): Error creating thread -- aborting\n");
            return -1;
        }
  
        DTRACE("Created thread %c\n", tpool.threads[i] -> id);
    }

    return 0;
}

/** Handles creating the unbounded queue and initializing the locks.
 * 
 * len: The initial length of the queue.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int queue_init(int len) {

    size_t init_len = len;
    tpool.queue = &queue;
    queue.buffer = (int *) malloc(init_len * sizeof(int));


    if (queue.buffer == NULL) {
        perror("queue_init(): Failed to allocate memory for queue buffer");
        return -1;
    }

    /* Create memory for the semaphore. */
    queue.has_jobs = (semaphore*) malloc(sizeof(semaphore));
    if (queue.has_jobs == NULL) {
        perror("queue_init(): Failed to allocate memory for semaphore");
        return -1;
    }

    /* Create an empty queue. */
    queue.len = init_len;
    queue.head = 0;
    queue.tail = 0;

    /* Initialize the locks and set waiting. */
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_mutex_init(&(queue.has_jobs -> mutex), NULL);
    pthread_cond_init(&(queue.has_jobs -> condition), NULL);
    queue.has_jobs -> flag = 0;

    return 0;
}

/** Handles creating a thread and assigning it a friendly name.
 * 
 * threadptr: A double pointer to a thread.
 * ord: The order of the thread. Increments off character A.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int thread_init(thread** threadptr, int ord) {

    *threadptr = (thread*) malloc(sizeof(thread));
    if (threadptr == NULL) {
        perror("(thread_init): Failed to allocate memory for thread");
        return -1;
    }

    /* Give them simple names A, B, C, ..., Z. */
    (*threadptr) -> id = 'A' + ord;

    /* Create the threads and detach them from the process. */
    pthread_create(&((*threadptr) -> pthread_id), NULL, thread_loop, (*threadptr));
    pthread_detach((*threadptr) -> pthread_id);

    return 0;
  }

/** A task consumer which removes tasks off the queue (if any) and performs the task.
 * 
 * thr: A thread which will perform a task.
 * 
 * Returns: None.
*/
static void* thread_loop(void *thr) {

    int task;

    /* DEBUG: Required to print the thread info. */
//    thread *_thread;
//    _thread = (thread *) thr;

    /* If the queue is empty, wait. Then run any new jobs. */
    while(1) {
        pool_wait(tpool.queue -> has_jobs);
        pthread_mutex_lock(&tpool.queue -> mutex);
        task = dequeue(tpool.queue);
//        DTRACE("Thread %c: Received %d\n", _thread -> id, task);
        pthread_mutex_unlock(&tpool.queue -> mutex);

        tpool.subroutine(task);

//        DTRACE("Thread %c: Completed %d\n", _thread -> id, task);
    }

    return NULL;
}

/** Handles the waiting when there isn't anything going on.
 * 
 * sem: A semaphore associated with the queue.
 * 
 * Returns: None.
*/
static void pool_wait(semaphore* sem) {

    pthread_mutex_lock(&sem -> mutex);

    while(sem -> flag == 0) {
        pthread_cond_wait(&sem -> condition, &sem -> mutex);
    }

    sem -> flag -= 1;
    pthread_mutex_unlock(&sem -> mutex);
}

/** Removes a task from the queue.
 * 
 * q: The queue to which a task will be removed.
 * 
 * Returns: An integer representing the task to be completed.
*/
static int dequeue(job_queue *q) {
    
    int task = q -> buffer[q -> head];
    q -> head = (q -> head + 1) % q -> len;

    return task;
}

/** Public interface for adding a task to a pool.
 * 
 * task: The particular task which is to be added.
 * 
 * Returns: An integer representing the task to be completed.
*/
int tpool_add_task(int task) {
    int rval;

    /* Add a task to the pool. */
    pthread_mutex_lock(&tpool.queue -> mutex);
    rval = enqueue(tpool.queue, task);
    pthread_mutex_unlock(&tpool.queue -> mutex);

    return rval;
}

/** Adds a task to the queue. Resizes the queue if necessary. Signals that there is a new job.
 * 
 * q: The queue to which the task will be added.
 * task: The particular task which is to be added.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int enqueue(job_queue *q, int task) {

    /* Check if the queue is full. */
    if(((q -> tail + 1) % (int)q -> len) == q -> head) {
        if(resize_queue(q)) {
            perror("(enqueue) resize_queue(): Failed to increase the size of the queue.");
            return -1;
        }
    }

    /* Add the task to the end of the queue. */
    q -> buffer[q -> tail] = task;

    /* Move the tail down. */
    q -> tail = (q -> tail + 1) % q -> len;

//    DTRACE("Queue: Received %d\n", task);

    /* Signal that there is a new task */
    pthread_mutex_lock(&q -> has_jobs -> mutex);
    queue.has_jobs -> flag += 1;
    pthread_cond_signal(&q -> has_jobs -> condition);
    pthread_mutex_unlock(&q -> has_jobs -> mutex);

    return 0;
}

/** Resizes the queue by doubling its size.
 * 
 * q: The queue which should be resized.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
static int resize_queue(job_queue *q) {

    DTRACE("Resizing queue.");

    static int i;
    size_t newLen = q -> len * 2;
    q -> buffer = (int *) realloc(q -> buffer, newLen * sizeof(int));

    if(q -> buffer == NULL) {
        perror("(resize_queue): Failed to realloc memory for the new buffer.");
        return -1;
    }

    /* The head is past the tail */
    if(q -> head > q -> tail) {
        /* Move the head portion in front of the queue */
        for(i = q-> head; i < (int) q -> len; i++) {
            q -> buffer[q -> len + 1] = q -> buffer[i];     /* Don't forget len is the old len. */
        }
        q -> head += q -> len;
    }

    /* Set the new length to the queue parameter */
    q -> len = newLen;

    return 0;
}
