/*
 * =====================================================================================
 *
 *       Filename:  threadlib.c
 *
 *    Description:  
 *
 
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "threadlib.h"

thread_t *
create_thread(thread_t *thread,
			  char *name) {

	if (!thread) {
		thread = malloc(sizeof(thread_t));
	}

	memset(thread, 0 , sizeof(thread_t));
	strncpy(thread->name, name, sizeof(thread->name));
	pthread_cond_init(&thread->cond_var, 0);
	pthread_attr_init(&thread->attributes);
	thread->block_status = false;
	pthread_mutex_init(&thread->thread_state_mutex, 0);
	return thread;
}

void
run_thread(thread_t *thread,
		   void *(*thread_fn)(void *),
		   void *arg){

	thread->thread_fn = thread_fn;
	pthread_attr_init(&thread->attributes);
	pthread_attr_setdetachstate(&thread->attributes, PTHREAD_CREATE_JOINABLE);
	pthread_create(&thread->thread, &thread->attributes,
				thread_fn, arg);
}

