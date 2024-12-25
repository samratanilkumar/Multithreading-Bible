/*
 * =====================================================================================
 *
 *       Filename:  threadlib.h
 *
 *    Description:  This file defines the commonly used data structures and routines for
 	for thread synchronization
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

typedef struct thread_{

    char name[32];
    bool thread_created;
    pthread_t thread;
    void *arg;
    void *(*thread_fn)(void *);
    pthread_attr_t attributes;
} thread_t;

thread_t *
thread_create(thread_t *thread, char *name);

void
thread_run(thread_t *thread, void *(*thread_fn)(void *), void *arg);

void
thread_set_thread_attribute_joinable_or_detached(
            thread_t *thread, bool joinable);

#endif /* __THREAD_LIB__  */

/*
  Visit : www.csepracticals.com for more courses and projects
  Join Telegram Grp : telecsepracticals
*/

