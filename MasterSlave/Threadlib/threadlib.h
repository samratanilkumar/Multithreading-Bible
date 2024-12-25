/*
 * =====================================================================================
 *
 *       Filename:  threadlib.h
 *
 *    Description:  
 *
 * =====================================================================================
 */

#ifndef __THREAD_LIB__
#define __THREAD_LIB__

#include <stdbool.h>
#include <pthread.h>

typedef struct thread_{

    char name[32];
    pthread_t thread;
    void *(*thread_fn)(void *);
    pthread_cond_t cond_var;
    pthread_attr_t attributes;
	bool block_status;
	pthread_mutex_t thread_state_mutex;
} thread_t;

thread_t *
create_thread(thread_t *thread, char *name);

void
run_thread(thread_t *thread, void *(*thread_fn)(void *), void *arg);


#endif /* __THREAD_LIB__  */
