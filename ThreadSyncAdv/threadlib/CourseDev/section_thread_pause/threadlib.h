/*
 * =====================================================================================
 *
 *       Filename:  threadlib.h
 *
 *    Description:  This file defines the commonly used data structures and routines for
 	for thread synchronization
 
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
} thread_t;

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

#endif /* __THREAD_LIB__  */

/*
  Visit : www.csepracticals.com for more courses and projects
  Join Telegram Grp : telecsepracticals
*/

