/*
 * =====================================================================================
 *
 *       Filename:  threadlib.h
 *
 *    Description:  This file defines the commonly used data structures and routines for
 	for thread synchronization
 *
 
 *
 * =====================================================================================
 */

/*
  Visit : www.csepracticals.com for more courses and projects
  Join Telegram Grp : telecsepracticals
*/

#ifndef __THREAD_LIB__
#define __THREAD_LIB__

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <semaphore.h>

/* When the thread is running and doing its work as normal */
#define THREAD_F_RUNNING            (1 << 0)
/* When the thread has been marked to pause, but not paused yet */
#define THREAD_F_MARKED_FOR_PAUSE   (1 << 1)
/* When thread is blocked (paused) */
#define THREAD_F_PAUSED             (1 << 2)
/* When thread is blocked on CV for reason other than paused */
#define THREAD_F_BLOCKED            (1 << 3)

typedef struct thread_{
	/*name of the thread */
    char name[32];
	/* is execution unit has been created*/
    bool thread_created;
	/* pthread handle */
    pthread_t thread;
	/* thread fn arg */
    void *arg;
	/* thread fn */
    void *(*thread_fn)(void *);
    /* Fn to be invoked just before pauing the thread */
    void *(*thread_pause_fn)(void *);
    /* Arg to be supplied to pause fn */
    void *pause_arg;
    /* track thread state */
    uint32_t flags;
    /* update thread state mutually exclusively */
    pthread_mutex_t state_mutex;
    /* cv on which thread will block itself*/
    pthread_cond_t cv;
    /* thread Attributes */
    pthread_attr_t attributes;
    sem_t *semaphore;
    glthread_t wait_glue;
} thread_t;
GLTHREAD_TO_STRUCT(wait_glue_to_thread,
                thread_t, wait_glue);


thread_t *
thread_create(thread_t *thread, char *name);

void
thread_run(thread_t *thread, void *(*thread_fn)(void *), void *arg);

void
thread_set_thread_attribute_joinable_or_detached(
            thread_t *thread, bool joinable);


/* Thead pausing and resuming */

void
thread_set_pause_fn(thread_t *thread,
                    void *(*thread_pause_fn)(void *),
                    void *pause_arg);

void
thread_pause(thread_t *thread);

void
thread_resume(thread_t *thread);

void
thread_test_and_pause(thread_t *thread);



/* Thread Pool Begin */


typedef struct thread_pool_ {

  glthread_t pool_head;
  pthread_mutex_t mutex;
} thread_pool_t;


typedef struct thread_execution_data_ {

    void *(*thread_stage2_fn)(void *);
    void *stage2_arg;

    void (*thread_stage3_fn)(thread_pool_t *, thread_t *);
    thread_pool_t *thread_pool;
    thread_t *thread;

} thread_execution_data_t;


void
thread_pool_init (thread_pool_t *th_pool );

void
thread_pool_insert_new_thread (thread_pool_t *th_pool, thread_t *thread);

thread_t *
thread_pool_get_thread (thread_pool_t *th_pool);

void
thread_pool_dispatch_thread (thread_pool_t *th_pool,     
                            void *(*thread_fn)(void *),
                            void *arg, bool block_caller);



/* Wait Queues Implementation Starts here */
typedef struct cv_ {

    int id;
    pthread_cond_t cv;
    glthread_t glue; /* list node */
} cv_t;

static inline cv_t *
cv_get_new_cv () {
    static int cv_id = 0;
    cv_t *cv = (cv_t *)calloc(1, sizeof(cv_t));
    pthread_cond_init(&cv->cv, NULL);
    init_glthread(&cv->glue);
    cv_id++;
    cv->id = cv_id;
    return cv;
}

GLTHREAD_TO_STRUCT(glnode_to_cv, cv_t , glue);

typedef struct wait_queue_ {

    /*  No of threads waiting in a wait-queue*/
    uint32_t thread_wait_count;

    /*  CV on which multiple threads in wait-queue are   blocked */
    pthread_cond_t cv;

    /*  Application owned Mutex cached by wait-queue */
    pthread_mutex_t *appln_mutex;

    /* Head of FIFO Queue of CVs on which threads are blocked */
    glthread_t fifo_thread_q;
    /* Is this WQ a FIFO WQ */
    bool is_fifo_enabled;
} wait_queue_t;


typedef bool (*wait_queue_condn_fn)
      (void *appln_arg, pthread_mutex_t **out_mutex);

void
wait_queue_init (wait_queue_t * wq, bool is_fifo);


thread_t *
wait_queue_test_and_wait (wait_queue_t *wq,
        wait_queue_condn_fn wait_queue_block_fn_cb,
        void *arg );


void
wait_queue_signal (wait_queue_t *wq, bool lock_mutex);


void
wait_queue_broadcast (wait_queue_t *wq, bool lock_mutex);


void
wait_queue_destroy (wait_queue_t *wq);


/* Wait Queues Implementation Ends here */





#endif /* __THREAD_LIB__  */

/*
  Visit : www.csepracticals.com for more courses and projects
  Join Telegram Grp : telecsepracticals
*/

