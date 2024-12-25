/*
 * =====================================================================================
 *
 *       Filename:  sema.h
 *
 *    Description: This file defines the declaration of Semaphores 
 *

 * =====================================================================================
 */

#ifndef __SEMA__
#define __SEMA__

typedef struct sema_ sema_t;

struct sema_ {

    int permit_counter;
    pthread_cond_t cv;
    pthread_mutex_t mutex;
};

sema_t *
sema_get_new_semaphore();

void
sema_init(sema_t *sema, int count);

void
sema_wait(sema_t *sema);

void
sema_post(sema_t *sema);

void
sema_destroy(sema_t *sema);

int
sema_getvalue(sema_t *sema);

/*
 * Several texts using P & V notation to represent
 * wait and signals
 */
#define P(sema) sema_wait(sema)
#define V(sema)	sema_post(sema)

/*
 * Some texts also use up and down
 */
#define UP(sema)	sema_wait(sema)
#define DOWN(sema)	sema_post(sema)

#endif /* __SEMA__ */
