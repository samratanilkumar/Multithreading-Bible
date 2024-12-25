/*
 * 0ty
 * =====================================================================================
 *
 *       Filename:  threadlib.c
 *
 *    Description:  This file implements the commonly used data structures and routines for
 for thread synchronization
 *
 
 *
 * =====================================================================================
 */

/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include "threadlib.h"

/* Fn to create and initialize a new thread Data structure
   When a new thread_t is created, it is just a data structure
   sitting in the memory, not an execution unit. The actual execution
   unit is thread_t->thread which we shall be creating using
   pthread_create( ) API later
   */
thread_t *
create_thread(thread_t *thread,
        char *name,
        thread_op_type_t thread_op) {

    if (!thread) {
        thread = calloc (1, sizeof(thread_t));
    }

    strncpy(thread->name, name, sizeof(thread->name));  
    thread->thread_created = false;
    thread->arg = NULL;
    thread->semaphore = NULL;
    thread->thread_fn = NULL;
    pthread_cond_init(&thread->cv, 0);
    pthread_attr_init(&thread->attributes);
    thread->thread_op = thread_op;
    init_glthread(&thread->wait_glue);
    return thread;
}

void
run_thread(thread_t *thread,
        void *(*thread_fn)(void *),
        void *arg){

    thread->thread_fn = thread_fn;
    thread->arg = arg;
    pthread_attr_init(&thread->attributes);
    pthread_attr_setdetachstate(&thread->attributes, PTHREAD_CREATE_JOINABLE);
    thread->thread_created = true;
    pthread_create(&thread->thread,
            &thread->attributes,
            thread_fn,
            arg);
}

void
thread_lib_print_thread(thread_t *thread) {

    printf("thread->name = %s\n", thread->name);
    printf("thread->thread_created = %s\n",
            thread->thread_created ? "Y" : "N");
    printf("thread->thread_op = %u\n", thread->thread_op);
}




/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/







/* Thread Pool Implementation Starts from here*/

static void
thread_pool_run_thread (thread_t *thread) {
    /* 
       This fn assumes that thread has already been removed from
       thread pool
       */
    assert (IS_GLTHREAD_LIST_EMPTY(&thread->wait_glue));

    if (!thread->thread_created) {
        run_thread(thread, thread->thread_fn, thread->arg);
    }
    else {
        /* If the thread is already created, it means the thread is
           in blocked state as it has completed its previous task. So
           resume the thread execution again
           */
        pthread_cond_signal(&thread->cv);
    }
}

static void
thread_pool_thread_stage3_fn(thread_pool_t *th_pool,
        thread_t *thread) {

    pthread_mutex_lock(&th_pool->mutex);

    glthread_priority_insert (&th_pool->pool_head,
            &thread->wait_glue,
            th_pool->comp_fn,
            (size_t)&(((thread_t *)0)->wait_glue));

    /* Tell the caller thread (which dispatched me from pool) that in
       am done */
    if (thread->semaphore){
        sem_post(thread->semaphore);
    }

    /* Rest in peace again in thread pool after completing the task*/
    pthread_cond_wait(&thread->cv, &th_pool->mutex);
    pthread_mutex_unlock(&th_pool->mutex);
}

static void *
thread_fn_execute_stage2_and_stage3(void *arg) {

    thread_execution_data_t *thread_execution_data =
        (thread_execution_data_t *)arg;

    while(true) {
        /* Stage 2 : USer defined function with user defined argument*/
        thread_execution_data->thread_stage2_fn (thread_execution_data->stage2_arg);
        /*  Stage 3 : Queue the thread in thread pool and block it*/
        thread_execution_data->thread_stage3_fn (thread_execution_data->thread_pool, 
                thread_execution_data->thread);
    }
}

/* This fn trigger the thread work cycle which involves :
   1. Stage 1 : Picking up the thread from thread pool
   Set up the Zero semaphore (optionally) if parent thread needs to blocked
   until the worker thread finishes the task
   2. Setup stage 2 and stage 3
   2.a : Stage 2 - Thread does the actual task assigned by the appln
   2.b : Stage 3 - Thread park itself back in thread pool and notify the
   appln thread (parent thread) if required

   thread_execution_data_t is a data structure which stores the functions (unit of work) along
   with the data (arguments) to be processed by worker thread in stage 2 and stage 3
   */
static void
thread_pool_thread_stage1_fn (thread_pool_t *th_pool,
        void *(*thread_fn)(void *),
        void *arg,
        bool block_caller) {

    sem_t *sem0_1 = NULL;

    /* get the thread from the thread pool*/
    thread_t *thread = thread_pool_get_thread (th_pool);

    if (!thread) {
        return;
    }

    if (block_caller) {
        sem0_1 = calloc(1, sizeof(sem_t));
        sem_init(sem0_1, 0, 0);
    }

    /* Cache the semaphore in the thread itself, so that when
       thread finishes the stage 3, it can notify the parent thread
    */
    thread->semaphore = sem0_1 ? sem0_1 : NULL;

    thread_execution_data_t *thread_execution_data = 
        (thread_execution_data_t *)(thread->arg);

    if (thread_execution_data == NULL ) {
    /* In this data structure, we would wrap up all the information
        which thread needs to execute stage 2 ans stage 3
    */
        thread_execution_data = calloc (1, sizeof(thread_execution_data_t));
    }

    /* Setup Stage 2 in which thread would do assigned task*/
    thread_execution_data->thread_stage2_fn = thread_fn;
    thread_execution_data->stage2_arg = arg;

    /* Setup Stage 3 in which thread would park itself in thread pool */
    thread_execution_data->thread_stage3_fn = thread_pool_thread_stage3_fn;
    thread_execution_data->thread_pool = th_pool;
    thread_execution_data->thread = thread;

    /* Assign the aggregate work to the thread to perform i.e. Stage 2 followed
       by stage 3 */
    thread->thread_fn = thread_fn_execute_stage2_and_stage3;
    thread->arg = (void *)thread_execution_data;

    /* Fire the thread now */
    thread_pool_run_thread (thread);

    if (block_caller) {
        /* Wait for the thread to finish the Stage 2 and Stage 3 work*/
        sem_wait(sem0_1);
        /* Caller is notified , destory the semaphore */
        sem_destroy(sem0_1);
        free(sem0_1);
        sem0_1 = NULL;
        thread->semaphore = NULL;
    }
}

void
thread_pool_dispatch_thread (thread_pool_t *th_pool,
        void *(*thread_fn)(void *),
        void *arg,
        bool block_caller) {

    thread_pool_thread_stage1_fn (th_pool, thread_fn, arg, block_caller);
}

void
thread_pool_init(thread_pool_t *th_pool,
        int (*comp_fn)(void *, void *)) {

    init_glthread(&th_pool->pool_head);
    th_pool->comp_fn = comp_fn;
    pthread_mutex_init(&th_pool->mutex, NULL);
}

void
thread_pool_insert_new_thread(thread_pool_t *th_pool,
        thread_t *thread) {


    pthread_mutex_lock(&th_pool->mutex);

    assert (IS_GLTHREAD_LIST_EMPTY(&thread->wait_glue));
    assert(thread->thread_fn == NULL);

    glthread_priority_insert (&th_pool->pool_head,
            &thread->wait_glue,
            th_pool->comp_fn,
            (size_t)&(((thread_t *)0)->wait_glue));

    pthread_mutex_unlock(&th_pool->mutex);
}

thread_t *
thread_pool_get_thread(thread_pool_t *th_pool) {

    thread_t *thread = NULL;
    glthread_t *glthread = NULL;

    pthread_mutex_lock(&th_pool->mutex);
    glthread = dequeue_glthread_first(&th_pool->pool_head);
    if (!glthread) {
        pthread_mutex_unlock(&th_pool->mutex);
        return NULL;
    }
    thread = wait_glue_to_thread(glthread);
    pthread_mutex_unlock(&th_pool->mutex);
    return thread;
}

#if 0
/* Main application using thread pool starts here */

/* Comparison fn to enqueue the thread in thread-pool. Returning -1 means
   newly added thread would queue up in the front of the thread pool list
   */
int
thread_pool_thread_insert_comp_fn(void *thread1, void *thread2){

    return -1;
}

void *
even_thread_work(void *arg) {

    int i;
    for (i = 0; i < 10; i+=2) {
        printf("even = %d\n", i);
        sleep(1);
    }
}

void *
odd_thread_work(void *arg) {

    int i;
    for (i = 1; i < 10; i+=2) {
        printf("odd = %d\n", i);
        sleep(1);
    }
}

int
main(int argc, char **argv) {

    /* Create and initialze a thread pool */
    thread_pool_t *th_pool = calloc(1, sizeof(thread_pool_t));
    thread_pool_init(th_pool, thread_pool_thread_insert_comp_fn);

    /* Create two threads (not execution units, just thread_t data structures) */
    thread_t *thread1 = create_thread(0, "even_thread", THREAD_WRITER);
    thread_t *thread2 = create_thread(0, "odd_thread", THREAD_WRITER);

    /* Insert both threads in thread pools*/
    thread_pool_insert_new_thread(th_pool, thread1);
    thread_pool_insert_new_thread(th_pool, thread2);

    thread_pool_dispatch_thread(th_pool, even_thread_work, 0, false);
    thread_pool_dispatch_thread(th_pool, odd_thread_work,  0, false);

    pthread_exit(0);
    return 0;
}

#endif






/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/








/* Event Pair Implementation Begin */

void
event_pair_server_init (event_pair_t *ep) {

    sem_init(&ep->server_sem0, 0, 0);
    /* It is suffice to initialize the initial_sync_sem0 in server_init
       fn since it is to be initialized only once, and we usually start servers
       before clients*/
    sem_init(&ep->initial_sync_sem0, 0, 0);
}

void
event_pair_client_init (event_pair_t *ep) {

    sem_init(&ep->client_sem0, 0, 0);
}

/* Blocks the calling thread */
void
event_pair_server_wait (event_pair_t *ep) {

    sem_wait(&ep->server_sem0);
}

void
event_pair_client_wait (event_pair_t *ep) {

    sem_wait(&ep->client_sem0);
}

/* Signal the other party, and block yourself */
void
event_pair_server_handoff (event_pair_t *ep) {

    sem_post(&ep->client_sem0);
}

void
event_pair_client_handoff (event_pair_t *ep) {

    sem_post(&ep->server_sem0);
}

void
event_pair_server_destroy (event_pair_t *ep) {

    sem_destroy(&ep->server_sem0);
}

void
event_pair_client_destroy (event_pair_t *ep) {

    sem_destroy(&ep->client_sem0);
}

void
event_pair_initial_sync_wait (event_pair_t *ep) {

    sem_wait(&ep->initial_sync_sem0);
}

void
event_pair_initial_sync_signal (event_pair_t *ep) {

    sem_post(&ep->initial_sync_sem0);
}

#if 0
static uint32_t global_shared_memory  = 1;

void *
server_fn(void *arg) {

    event_pair_t *ep = (event_pair_t *)arg;
    event_pair_initial_sync_signal ( ep );

    while(true) {
        /* Wait for client request now */
        printf("Server thread waiting client's next request\n");
        event_pair_server_wait(ep);

        printf("Server thread recvd client request = %u\n",
                global_shared_memory);

        /* Pick the request from shared memory and respond*/
        printf("Server thread processed client's request\n");
        global_shared_memory *= 2;

        event_pair_server_handoff(ep);
        printf("Server thread handoff to client\n");	
    }
}

void *
client_fn(void *arg) {

    event_pair_t *ep = (event_pair_t *)arg;

    while(true) {

        printf("client thread prepared next request = %u to send to server\n",
                global_shared_memory);

        event_pair_client_handoff(ep);
        printf("Client thread handoff to server\n");

        printf("client thread waiting for response from server\n");
        event_pair_client_wait(ep);

        /* process the server response here*/
        printf("client thread recvd server's response = %u\n",
                global_shared_memory);

        printf("client thread processed server's response\n");
        /* Pick the request from shared memory and respond*/
        if (global_shared_memory > 1024) {
            exit(0);
        }
    }
}

int
main(int argc, char **argv) {


    /* Create two threads (not execution units, just thread_t data structures) */
    thread_t *client_thread = create_thread(0, "client_thread", THREAD_WRITER);
    thread_t *server_thread = create_thread(0, "server_thread", THREAD_WRITER);

    /* Create and initialize the Event Pair Object */
    event_pair_t *ep = calloc(1, sizeof(event_pair_t));
    event_pair_server_init ( ep );

    /* Run the Server Thread */
    run_thread ( server_thread, server_fn, (void *)ep );

    /* Wait for the server thread to start and wait for the 
       request from client*/
    event_pair_initial_sync_wait(ep);

    event_pair_client_init(ep);
    /* We are here, means the server thread is blocked and waiting for
       request from client*/
    run_thread ( client_thread, client_fn, (void *)ep );

    pthread_exit(0);
    return 0;
}

/* More Ques :
   Implement the below scheme between client and server as follows :
   1. client sends 3 integers to server one by one - perpendicular (P), hypotenuse(H), and base(B).
   2. Server do not responds to client until it recvs all three integers from client
   3. Once Server recvs all three integers, server replies to client stating whether 
   P, H and B forms a right angle triangle or not. Server sends only boolean as a response
   to client
   4. Client prints the response recvd from server in the following format : 
   < Sq(H)  = Or !=  Sq(P) + Sq(B) >   ( = Or != Sign needs to be printed based on server boolean output recvd) 
   For ex : Sq(5) = Sq(3) + Sq(4)
   5. You can use random number generation function on the client side to generate integers representing H, P and B 
   (within range of [1, 500]). Google it.
   6. You are not suppose to modify the library code !
   */
#endif

/* Event Pair Implementation End */







/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/










/* Wait Queue Implementation Starts Here */

static int
priority_wq_insert_comp_fn(void *new_user_data, void *user_data_from_list) {

    thread_t *thread1 = (thread_t *)new_user_data;
    thread_t *thread2 = (thread_t *)user_data_from_list;

    /* If we return 1, then Queue becomes FIFO */
    return 1;
}

void
wait_queue_init (wait_queue_t * wq, bool priority_flag,
        int (*insert_cmp_fn)(void *, void *)) {

    wq->priority_flag = priority_flag;
    wq->thread_wait_count = 0;
    pthread_cond_init (&wq->cv, NULL);
    wq->appln_mutex = NULL;
    init_glthread(&wq->priority_wait_queue_head);
    wq->insert_cmp_fn = insert_cmp_fn;
    if (wq->priority_flag) {
        assert(wq->insert_cmp_fn);
    }
    wait_queue_set_auto_unlock_appln_mutex(wq, true);
}

/* Wait Queue unlocks/releases the application mutex on its own when thread
   is signalled successfully. In some cases (Monitor Implementation), we want
   to defer the responsibility to unlock the appln mutex to the application 
   it-self instead of wait Queue. This API overrides the behavior of the wait-queuesregaring
   regaring unlocking the appln mutex. Default behavior is wait-queue itself 
   look after to release the appln mutex when thread is unblocked from wait-queue
   */
void
wait_queue_set_auto_unlock_appln_mutex(wait_queue_t *wq, bool state) {

    wq->auto_unlock_appln_mutex = state;
}

/* All the majic of the Wait-Queue Thread-Synch Data structure lies in this
   API call. ISt arg is a wait-queue, 2nd arg is a ptr to the application fn
   the result (bool result) of which decides whether the calling thread need
   to block on wait_queue or not. The 3rd param is the argument to a fn.
   This fn returns the threads which has been signalled from WQ. This threads
   Can be different from curr_thread (last arg)
   */
thread_t *
wait_queue_test_and_wait (wait_queue_t * wq,
        wait_queue_block_fn wait_queue_block_fn_cb,
        void *arg,
        thread_t *curr_thread) {

    bool should_block;
    glthread_t *node;
    thread_t *return_thread;
    pthread_mutex_t *locked_appln_mutex = NULL;

    return_thread = NULL;

    /* If it is PQ, then passing the curr_thread ptr is mandatory */
    if (wq->priority_flag) {
        assert(curr_thread);
    }

    return_thread = curr_thread;

    /* Invoke the application fn to decide whether the calling thread
       needs to be blocked. This fn must lock the application mutex, and test
       the appln specific condn and return true or false without unlocking
       the mutex */
    should_block = wait_queue_block_fn_cb (arg, &locked_appln_mutex);

    /* Application must return the mutex which it has already locked*/
    assert (locked_appln_mutex);

    if (!wq->appln_mutex)
    {
        /* Catch the application mutex, WQ would need this for signaling
           the blocked threads on WQ*/
        wq->appln_mutex = locked_appln_mutex;
        printf("curr_thread = %s, wq = %p, wq->appln_mutex cached = %p\n",
                curr_thread->name, wq, wq->appln_mutex);
    }
    else
    {
        assert (wq->appln_mutex == locked_appln_mutex);
    }

    /* Conventional While loop which acts on predicate, and accordingly block
       the calling thread by invoking pthread_cond_wait*/
    while (should_block) {

        assert(wq->appln_mutex);
        wq->thread_wait_count++;
        printf("Thread %s blocked on wq, wq = %p, thread_wait_count incre to %u\n",
                curr_thread->name, wq, wq->thread_wait_count);

        /* If this is normal WQ, then block all threads on a common CV */
        if (!wq->priority_flag) {
            pthread_cond_wait (&wq->cv, wq->appln_mutex);
            /* We will not know which thread would wake up, hence return NULL.*/
            return_thread = NULL; 
        }
        else {
            /* If it is a Priority Wait Queue, then block all threads on their
               respective CV owned by the thread it-self */
            glthread_priority_insert (&wq->priority_wait_queue_head,
                    &curr_thread->wait_glue,
                    wq->insert_cmp_fn,
                    (size_t)&(((thread_t *)0)->wait_glue));
            printf("Thread %s blocked on wait Queue, wq->thread_wait_count = %u\n",
                    curr_thread->name, wq->thread_wait_count);
            pthread_cond_wait (&curr_thread->cv, wq->appln_mutex);
        }
        wq->thread_wait_count--;

        if (curr_thread) {
            printf("Thread %s un-blocked from wait Queue, wq->thread_wait_count reduced to = %u\n",
                    curr_thread->name, wq->thread_wait_count);
        }

        if (wq->priority_flag) {
            remove_glthread(&curr_thread->wait_glue);
            /*
               node = dequeue_glthread_first(
               &wq->priority_wait_queue_head);
               assert(node);
               curr_thread = wait_glue_to_thread(node);
               */
            return_thread = curr_thread;
        }
        /* The thread wakes up, retest the predicate again to 
           handle spurious wake up. Not that, appln need not test the
           block condition by locking the mutex this time since mutex is
           already locked in yhe first invocation of wait_queue_block_fn_cb()
           Hence Pass NULL as 2nd arg which hints the application that it has
           to test the predicate without locking any mutex */
        should_block = wait_queue_block_fn_cb (arg, NULL);
    }

    /* The mutex must be unlocked by the application after taking the appropriate
       action on the thread which is signalled
       */
    if (wq->auto_unlock_appln_mutex){
        pthread_mutex_unlock (locked_appln_mutex);
    }

    return return_thread;
}

void
wait_queue_signal (wait_queue_t * wq, bool lock_mutex)
{

    glthread_t *first_node;
    thread_t *thread;

    if (lock_mutex && !wq->appln_mutex) return;

    if (lock_mutex) {
        pthread_mutex_lock(wq->appln_mutex);
    }

    if (!wq->thread_wait_count) {
        if (lock_mutex) pthread_mutex_unlock(wq->appln_mutex);
        return;
    }

    if (!wq->priority_flag) {
        pthread_cond_signal (&wq->cv);
    }
    else {

        first_node = glthread_get_first_node (&wq->priority_wait_queue_head);

        if (!first_node) {
            if (lock_mutex) pthread_mutex_unlock(wq->appln_mutex);
            return;
        }

        thread = wait_glue_to_thread(first_node);
        pthread_cond_signal(&thread->cv);
    }
    if (lock_mutex) pthread_mutex_unlock(wq->appln_mutex);
}

void
wait_queue_broadcast (wait_queue_t * wq, bool lock_mutex) {

    glthread_t *curr;
    thread_t *thread;

    if (lock_mutex && !wq->appln_mutex) return;

    if (lock_mutex) pthread_mutex_lock(wq->appln_mutex);

    if (!wq->thread_wait_count) {
        if (lock_mutex) pthread_mutex_unlock(wq->appln_mutex);
        return;
    }

    if (!wq->priority_flag) {
        pthread_cond_broadcast (&wq->cv);
    }
    else {

        ITERATE_GLTHREAD_BEGIN(&wq->priority_wait_queue_head, curr) {

            thread = wait_glue_to_thread(curr);
            pthread_cond_signal(&thread->cv);
        } ITERATE_GLTHREAD_END(&wq->priority_wait_queue_head, curr);
    }

    if (lock_mutex) pthread_mutex_unlock(wq->appln_mutex);
}

void
wait_queue_print(wait_queue_t *wq) {

    glthread_t *curr;
    thread_t *thread;

    printf("wq->thread_wait_count = %u\n", wq->thread_wait_count);
    printf("wq->appl_result = %p\n", wq->appln_mutex);

    if (0 && wq->priority_flag) {

        ITERATE_GLTHREAD_BEGIN(&wq->priority_wait_queue_head, curr) {

            thread = wait_glue_to_thread(curr);
            thread_lib_print_thread(thread);
        } ITERATE_GLTHREAD_END(&wq->wait_queue_head, curr);
    }
}



/* Application fn to test wait-Queue library*/
#if 0
static wait_queue_t wq;

static bool appl_result = true;

    bool
test_appln_condition_in_mutual_exclusive_way (void *app_arg,
        pthread_mutex_t ** mutex)
{

    static pthread_mutex_t app_mutex;
    static bool initialized = false;

    if (!initialized) {
        pthread_mutex_init (&app_mutex, NULL);
        initialized = true;
    }

    *mutex = &app_mutex;
    return appl_result;
}

    bool
test_appln_condition_without_lock (void *app_arg)
{
    return appl_result;
}

    bool
appln_cond_test_fn (void *app_arg, pthread_mutex_t **mutex)
{

    if (mutex) {
        return test_appln_condition_in_mutual_exclusive_way (app_arg, mutex);
    }
    return test_appln_condition_without_lock (app_arg);
}

    void *
thread_fn_callback (void *arg)
{
    thread_t *thread = (thread_t *)arg;
    printf ("Thread %s invoking wait_queue_test_and_wait( ) \n",
            thread->name);
    wait_queue_test_and_wait (&wq, appln_cond_test_fn, NULL, thread);
    pthread_mutex_unlock(&wq->appln_mutex);
    pthread_exit(NULL);
    return NULL;
}

static thread_t *threads[3];

    int
main (int argc, char **argv)
{
    wait_queue_init (&wq, true, priority_wq_insert_comp_fn);

    threads[0] = create_thread(0, "th1", THREAD_WRITER);
    run_thread(threads[0], thread_fn_callback, (void *)threads[0]);

    threads[1] = create_thread(0, "th2", THREAD_WRITER);
    run_thread(threads[1], thread_fn_callback, (void *)threads[1]);

    threads[2] = create_thread(0, "th3", THREAD_WRITER);
    run_thread(threads[2], thread_fn_callback, (void *)threads[2]);

    sleep(3);


    wait_queue_print(&wq);

    appl_result = false;
    wait_queue_signal(&wq);
    sleep(1);
    wait_queue_print(&wq);
    wait_queue_signal(&wq);
    sleep(1);
    wait_queue_print(&wq);
    wait_queue_signal(&wq);
    sleep(1);
    wait_queue_print(&wq);

    pthread_join (threads[0]->thread, NULL);
    printf("Thread %s joined\n", threads[0]->name);
    wait_queue_print(&wq);

    pthread_join (threads[1]->thread, NULL);
    printf("Thread %s joined\n", threads[1]->name);
    wait_queue_print(&wq);

    pthread_join (threads[2]->thread, NULL);
    printf("Thread %s joined\n", threads[2]->name);
    wait_queue_print(&wq);

    return 0;
}

#endif

/* Wait Queue Implementation Ends Here */






/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/








/* Thread Barrier Implementation Starts here */

void
thread_barrier_print(th_barrier_t *th_barrier) {

    printf("th_barrier->threshold = %u\n", th_barrier->threshold);
    printf("th_barrier->curr_wait_count = %u\n", th_barrier->curr_wait_count);
    printf("th_barrier->is_ready_again = %s\n", th_barrier->is_ready_again ? "true" : "false");
}

void
thread_barrier_init ( th_barrier_t *barrier, uint32_t count) {

    barrier->threshold = count;
    barrier->curr_wait_count = 0;
    pthread_cond_init(&barrier->cv, NULL);
    pthread_mutex_init(&barrier->mutex, NULL);
    barrier->is_ready_again = true;
    pthread_cond_init(&barrier->busy_cv, NULL);
}

void
thread_barrier_signal_all ( th_barrier_t *barrier) {

    pthread_mutex_lock (&barrier->mutex);

    if ( barrier->is_ready_again == false ||
        barrier->curr_wait_count == 0 ) {

        pthread_mutex_unlock (&barrier->mutex);
        return;
    }

    pthread_cond_signal(&barrier->cv);
    pthread_mutex_unlock (&barrier->mutex);	
}

void
thread_barrier_barricade ( th_barrier_t *barrier) {

    pthread_mutex_lock (&barrier->mutex);

    while (barrier->is_ready_again == false ) {
        pthread_cond_wait(&barrier->busy_cv, 
                &barrier->mutex);
    }

    if ( barrier->curr_wait_count + 1 == barrier->threshold ) {

        barrier->is_ready_again = false;
        pthread_cond_signal(&barrier->cv);
        pthread_mutex_unlock (&barrier->mutex);
        return;
    }

    barrier->curr_wait_count++;
    pthread_cond_wait(&barrier->cv, &barrier->mutex);
    barrier->curr_wait_count--;

    if (barrier->curr_wait_count == 0) {
        barrier->is_ready_again = true;
        pthread_cond_broadcast(&barrier->busy_cv);
    }
    else {
        pthread_cond_signal(&barrier->cv);
    }
    pthread_mutex_unlock (&barrier->mutex);
}

/* Thread Barrier Implementation Ends here */








/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/












/* Monitor Implementation Starts here */

static int
monitor_wq_comp_default_fn(void *node1, void *node2) {
    return 1; /* FIFO */
}

monitor_t *
init_monitor(monitor_t *monitor,
        char *resource_name,
        uint16_t n_readers_max_limit,
        uint16_t n_writers_max_limit,
        int (*monitor_wq_comp_fn_cb)(void *, void *)) {

    int (*monitor_wq_comp_fn)(void *, void *);

    monitor_wq_comp_fn = monitor_wq_comp_fn_cb ?
        monitor_wq_comp_fn_cb :
        monitor_wq_comp_default_fn;

    if(monitor == NULL) {
        monitor = calloc(1, sizeof(monitor_t));
    }

    strncpy(monitor->name, resource_name,
            sizeof(monitor->name));

    pthread_mutex_init(&monitor->monitor_talk_mutex, 0);

    wait_queue_init(&monitor->reader_thread_wait_q, true, monitor_wq_comp_fn);
    wait_queue_init(&monitor->writer_thread_wait_q, true, monitor_wq_comp_fn);

    monitor_set_wq_auto_mutex_lock_behavior(monitor, THREAD_WRITER, false);
    monitor_set_wq_auto_mutex_lock_behavior(monitor, THREAD_READER, false);

    init_glthread(&monitor->active_threads_in_cs);
    monitor->resource_status = MON_RES_AVAILABLE;
    monitor->n_readers_max_limit = n_readers_max_limit;
    monitor->n_writers_max_limit = n_writers_max_limit;

    monitor->strict_alternation = false;
    monitor->who_accessed_cs_last = THREAD_ANY;
    monitor->shutdown = false;
    monitor->is_deleted = false;
    return monitor;
}

static inline void
monitor_lock_monitor_talk_mutex(monitor_t *monitor) {

    pthread_mutex_lock(&monitor->monitor_talk_mutex);
}

static inline void
monitor_unlock_monitor_talk_mutex(monitor_t *monitor) {

    pthread_mutex_unlock(&monitor->monitor_talk_mutex);
}

/* 
   Inspect the Monitor attributes and decide if the resource is availble.Inspect
   We need monitor itself and a requester thread to take decision on this.
   */

typedef struct monitor_thread_pkg_ {

    monitor_t *monitor;
    thread_t *thread;
} monitor_thread_pkg_t;

static bool
monitor_is_resource_available(void *arg,
        pthread_mutex_t **mutex) {

    uint16_t n_max_accessors;
    bool rc;
    thread_t *requester_thread;

    monitor_thread_pkg_t *monitor_thread_pkg =
        (monitor_thread_pkg_t *)arg;

    monitor_t *monitor = monitor_thread_pkg->monitor;
    requester_thread = monitor_thread_pkg->thread;

    rc = false;

    /* We need to check if the resource is availble for a requester_thread */

    /* Lock the monitor state, since we are inspecting the monitor attributes */
    if (mutex) {
        /* WQ specification says, we must return bool from this fn with mutex being
           locked */
        monitor_lock_monitor_talk_mutex(monitor);
        *mutex = &monitor->monitor_talk_mutex;
    }

    printf("Monitor %s checking resource availablity for thread %s\n",
            monitor->name, requester_thread->name);

    print_monitor_snapshot(monitor);

    n_max_accessors = requester_thread->thread_op == THREAD_READER ?
        monitor->n_readers_max_limit :
        monitor->n_writers_max_limit;

    uint16_t max_curr_user_count = requester_thread->thread_op == THREAD_READER ?
        monitor->n_curr_readers :
        monitor->n_curr_writers;

    switch (monitor->resource_status) {

        case MON_RES_AVAILABLE:
            rc = (max_curr_user_count < n_max_accessors) ? true : false;

            /* Handling Strict Alternation */
            if (monitor->strict_alternation) {

                if (!rc) break;

                if (monitor->who_accessed_cs_last != THREAD_ANY) {
                    printf("strict_alternation : Last accessed by %s, current thread %s\n",
                            monitor->who_accessed_cs_last == THREAD_READER ?
                            "THREAD_READER" : "THREAD_WRITER",
                            requester_thread->thread_op == THREAD_READER ?
                            "THREAD_READER" : "THREAD_WRITER");
                }
                else {
                    printf("strict_alternation : Last accessed by THREAD_ANY, "
                            "current thread %s\n",
                            requester_thread->thread_op == THREAD_READER ?
                            "THREAD_READER" : "THREAD_WRITER");
                }

                switch (monitor->who_accessed_cs_last){
                    case THREAD_ANY:
                        break;
                    case THREAD_READER:
                        rc = (requester_thread->thread_op == THREAD_WRITER);
                        break;
                    case THREAD_WRITER:
                        rc = (requester_thread->thread_op == THREAD_READER);
                        break;
                }
            }		
            break;

        case MON_RES_BUSY_BY_READER:

            switch(requester_thread->thread_op) {

                case THREAD_READER:
                    rc =  (max_curr_user_count < n_max_accessors) ? true : false;
                    break;
                case THREAD_WRITER:
                    rc = false;
                    break;
                default: ;
            }
            break;

        case MON_RES_BUSY_BY_WRITER:

            switch(requester_thread->thread_op) {

                case THREAD_WRITER:
                    rc =  (max_curr_user_count < n_max_accessors) ? true : false;
                    break;
                case THREAD_READER:
                    rc = false;
                    break;
                default: ;
            }
            break;
        default : ;
    }

    printf("Result : Access allowed : %s\n", rc ? "Y" : "N");
    return rc;
}

static bool
monitor_is_resource_not_available(void *arg,
        pthread_mutex_t **mutex) {

    return !monitor_is_resource_available(arg, mutex);
}


void
monitor_check_and_delete_monitor(monitor_t *monitor) {
    /* Monitor mutex is already locked */
    if (monitor->writer_thread_wait_q.thread_wait_count ||
            monitor->reader_thread_wait_q.thread_wait_count ||
            monitor->resource_status != MON_RES_AVAILABLE) {
        return;	
    }
    monitor->is_deleted = true;
}

/* Caller is responsible to lock monitor's mutex */
static void
monitor_set_resource_status (monitor_t *monitor,
        thread_t *access_granted_thread) {

    if (!access_granted_thread) {
        monitor->resource_status = MON_RES_AVAILABLE;
        monitor_check_and_delete_monitor(monitor);
        return;
    }

    monitor->who_accessed_cs_last = access_granted_thread->thread_op;

    switch(access_granted_thread->thread_op) {

        case THREAD_READER:
            monitor->resource_status = MON_RES_BUSY_BY_READER;
            break;
        case THREAD_WRITER:
            monitor->resource_status = MON_RES_BUSY_BY_WRITER;
        default: ;
    }

    printf("monitor->resource_status set to %s, " 
            "monitor->who_accessed_cs_last set to %s\n",
            monitor->resource_status == MON_RES_BUSY_BY_WRITER ?
            "MON_RES_BUSY_BY_WRITER" : "MON_RES_BUSY_BY_READER",
            monitor->who_accessed_cs_last == THREAD_ANY ? "THREAD_ANY" : 
            monitor->who_accessed_cs_last == THREAD_READER ?
            "THREAD_READER" : "THREAD_WRITER");
}

void
monitor_request_access_permission(
        monitor_t *monitor,
        thread_t *requester_thread) {

    thread_t *thread;
    wait_queue_t *wq;
    uint16_t n_max_accessors = 0;

    assert(IS_GLTHREAD_LIST_EMPTY(&requester_thread->wait_glue));

    monitor_lock_monitor_talk_mutex(monitor);

    if (monitor->shutdown) {
        printf("Monitor %s reject access request to thread %s, "
                "Monitor being shut down\n", monitor->name, requester_thread->name);
        monitor_unlock_monitor_talk_mutex(monitor);
        return;
    }
    else {
        monitor_unlock_monitor_talk_mutex(monitor);
    }

    printf("Thread %s(%d) requesting Monitor %s for Resource accesse\n",
            requester_thread->name,
            requester_thread->thread_op,
            monitor->name);

    n_max_accessors = requester_thread->thread_op == THREAD_READER ?
        monitor->n_readers_max_limit :
        monitor->n_writers_max_limit;

    wq = requester_thread->thread_op == THREAD_READER ?
        &monitor->reader_thread_wait_q :
        &monitor->writer_thread_wait_q;

    monitor_thread_pkg_t monitor_thread_pkg = {monitor, requester_thread};

    thread = wait_queue_test_and_wait (wq, 
            monitor_is_resource_not_available,
            (void *)&monitor_thread_pkg,
            requester_thread);

    /* 
       Monitor is already locked here. Note that Monitor appears as appln
       to Monitor-owned Wait Queues.
       */

    printf("Monitor %s resource available, Thread %s granted Access\n",
            monitor->name, thread->name);

    init_glthread(&thread->wait_glue);
    glthread_add_next(&monitor->active_threads_in_cs, 
            &thread->wait_glue);

    uint16_t *max_curr_user_count = thread->thread_op == THREAD_READER ?
        &monitor->n_curr_readers :
        &monitor->n_curr_writers;

    uint16_t *switch_count = thread->thread_op == THREAD_READER ?
        &monitor->switch_from_writers_to_readers :
        &monitor->switch_from_readers_to_writers;

    (*max_curr_user_count)++;
    printf("No of accessors increased to %u\n", *max_curr_user_count);
    monitor_set_resource_status (monitor, thread);

    if (*max_curr_user_count == 1) {
        (*switch_count)++;
        printf("Monitor Switch count = [%u %u]\n",
                monitor->switch_from_readers_to_writers,
                monitor->switch_from_writers_to_readers);
    }

    /* Unlock the application mutex manually, here we are unlocking
       the monitor's talk mutex'*/
    if (!wq->auto_unlock_appln_mutex) {
        pthread_mutex_unlock(wq->appln_mutex);
    }
}

void
monitor_inform_resource_released(
        monitor_t *monitor,
        thread_t *requester_thread){

    monitor_lock_monitor_talk_mutex(monitor);

    printf("Thread %s(%d) informing Monitor %s for Resource release\n",
            requester_thread->name,
            requester_thread->thread_op,
            monitor->name);

    remove_glthread(&requester_thread->wait_glue);
    init_glthread(&requester_thread->wait_glue);

    switch(requester_thread->thread_op){

        case THREAD_READER:
            assert(monitor->n_curr_readers);
            monitor->n_curr_readers--;
            printf("# of readers accessing resource reduced to %u\n",
                    monitor->n_curr_readers);

            if (monitor->n_curr_readers == 0) {
                printf("Nobody using the resource, marking the resource Available\n");
                monitor_set_resource_status(monitor, NULL);
            }

            if (monitor->reader_thread_wait_q.thread_wait_count) {
                /* If some reader threads are waiting, signal one of them */
                printf("# of Reader thread waiting = %u, "
                        "Sending Signal to another Reader thread\n",
                        monitor->reader_thread_wait_q.thread_wait_count);
                wait_queue_signal(&monitor->reader_thread_wait_q, false);
            }
            else if (monitor->n_curr_readers == 0 &&
                    monitor->writer_thread_wait_q.thread_wait_count) {
                /* If some writer threads are waiting, broadcast all of them */
                printf("# of Writer thread waiting = %u, "
                        "Broadcasting Signal to all Writer threads\n",
                        monitor->writer_thread_wait_q.thread_wait_count);
                wait_queue_broadcast(&monitor->writer_thread_wait_q, false);
            }
            break;
        case THREAD_WRITER:
            assert(monitor->n_curr_writers);
            monitor->n_curr_writers--;
            printf("# of writers accessing resource reduced to %u\n",
                    monitor->n_curr_writers);

            if (monitor->n_curr_writers == 0) {
                printf("Nobody using the resource, marking the resource Available\n");
                monitor_set_resource_status(monitor, NULL);
            }

            if (monitor->writer_thread_wait_q.thread_wait_count) {
                /* If some writer threads are waiting, signal one of them */
                printf("# of Writer thread waiting = %u, "
                        "Sending Signal to another Writer thread\n",
                        monitor->writer_thread_wait_q.thread_wait_count);
                wait_queue_signal(&monitor->writer_thread_wait_q, false);
            }
            else if (monitor->n_curr_writers == 0 &&
                    monitor->reader_thread_wait_q.thread_wait_count) {
                /* If some reader threads are waiting, broadcast all of them */
                printf("# of Reader threads waiting = %u, "
                        "Broadcasting Signal to all Reader threads\n",
                        monitor->reader_thread_wait_q.thread_wait_count);
                wait_queue_broadcast(&monitor->reader_thread_wait_q, false);
            }
        default : ;
    }
    monitor_unlock_monitor_talk_mutex(monitor);

    /*  Only the last thread exiting out the monitor will see
        is_deleted flag set to true. No need to perform this operation
        under mutex lock (and not possible either) as there wont be any
        concurrent threads accessing the monitor at this point
        */
    if (monitor->is_deleted) {
        pthread_mutex_destroy(&monitor->monitor_talk_mutex);
        printf("Freeing the monitor\n");
        free(monitor);
    }
}

void
monitor_set_wq_auto_mutex_lock_behavior(
        monitor_t *monitor,
        thread_op_type_t thread_type,
        bool wq_auto_locking_state) {

    wait_queue_t *wq = (thread_type == THREAD_WRITER) ?
        &monitor->writer_thread_wait_q :
        &monitor->reader_thread_wait_q;

    wait_queue_set_auto_unlock_appln_mutex(wq, wq_auto_locking_state);
}

void
monitor_set_strict_alternation_behavior(monitor_t *monitor) {

    if (monitor->n_readers_max_limit == 1 &&
            monitor->n_writers_max_limit == 1) {
        monitor->strict_alternation = true;
        return;
    }
    assert(0);
}

void
print_monitor_snapshot(monitor_t *monitor) {

    printf("reader wait q count : %u, max limit : %u, curr readers : %u\n",
            monitor->reader_thread_wait_q.thread_wait_count,
            monitor->n_readers_max_limit,
            monitor->n_curr_readers);

    printf("writer wait q count : %u, max limit : %u, curr writers : %u\n",
            monitor->writer_thread_wait_q.thread_wait_count,
            monitor->n_writers_max_limit,
            monitor->n_curr_writers);
}

void
monitor_sanity_check(monitor_t *monitor) {

    if ((monitor->n_curr_readers > monitor->n_readers_max_limit) ||
            (monitor->n_curr_writers > monitor->n_writers_max_limit)) {
        assert(0);
    }
}

/* Fn to shut down the monitor, We cant cancel the threads which may be
   waiting in Monitor's Wait Queues as those threads may have other resources
   locked. The correct way to do so is , allow all waiting threads to access
   the resource one final time, and debar any new thread from submitting access
   request to a monitor. When monitor do not have any threads in the Wait-Queues
   or currently accessing the resource, then it shall be the safest point to clean
   and delete all monitor resources. 
   This is analogues to like how shop-keepers close their shops - Disallow new customers,
   allow the existing customers in the shop to finish up their shopping, and when
   all custp,ers// have gracefully exit the shop, then shut down the shop doors.
   This is Cool Stuff !!
   */

void
monitor_shut_down (monitor_t *monitor) {

    monitor_lock_monitor_talk_mutex(monitor);
    monitor->shutdown = true;
    monitor_unlock_monitor_talk_mutex(monitor);
}


#if 0
monitor_t *mon;

void *
thread_fn(void *arg) {

    thread_t *thread = (thread_t *)arg;

    while(1){

        printf("Thread %s Requesting Resource access\n",
                thread->name);

        monitor_request_access_permission(
                mon, thread);

        printf("Thread %s Accessing Resource\n",
                thread->name);

        //sleep(1);
        print_monitor_snapshot(mon);
        monitor_sanity_check(mon);


        printf("Thread %s informing Resource release\n",
                thread->name);

        monitor_inform_resource_released(
                mon, thread);

        printf("Thread %s Done with the Resource\n",
                thread->name);
    }
}


int
main(int argc, char **argv) {

    mon = init_monitor(0, "RT_TABLE", 0, 0, NULL);

    thread_t *thread1 = create_thread( 0, "Reader1",
            THREAD_READER);
    run_thread(thread1, thread_fn, thread1);

    thread_t *thread2 = create_thread( 0, "Reader2",
            THREAD_READER);
    run_thread(thread2, thread_fn, thread2);

    thread_t *thread3 = create_thread( 0, "Writer1",
            THREAD_WRITER);
    run_thread(thread3, thread_fn, thread3);

    thread_t *thread4 = create_thread( 0, "Writer2",
            THREAD_WRITER);
    run_thread(thread4, thread_fn, thread4);

    thread_t *thread5 = create_thread( 0, "Writer3",
            THREAD_WRITER);
    run_thread(thread5, thread_fn, thread5);

    thread_t *thread6 = create_thread( 0, "Reader3",
            THREAD_READER);
    run_thread(thread6, thread_fn, thread6);

    pthread_exit(0);
    return 0;
}

#endif

/* Using monitor to demonstrate producer consumer */
#if 0
monitor_t *mon;

void *
thread_fn(void *arg) {

    thread_t *thread = (thread_t *)arg;

    while(1){

        printf("Thread %s Requesting Resource access\n",
                thread->name);

        monitor_request_access_permission(
                mon, thread);

        printf("Thread %s Accessing Resource\n",
                thread->name);

        sleep(1);
        print_monitor_snapshot(mon);
        monitor_sanity_check(mon);


        printf("Thread %s informing Resource release\n",
                thread->name);

        monitor_inform_resource_released(
                mon, thread);

        printf("Thread %s Done with the Resource\n",
                thread->name);
    }
}


int
main(int argc, char **argv) {

    mon = init_monitor(0, "SHARED_BUFFER", 1, 1, NULL);
    monitor_set_strict_alternation_behavior(mon);

    thread_t *thread1 = create_thread( 0, "Consumer",
            THREAD_READER);
    run_thread(thread1, thread_fn, thread1);

    thread_t *thread2 = create_thread( 0, "Producer",
            THREAD_WRITER);
    run_thread(thread2, thread_fn, thread2);
#if 0
    sleep(10);
    monitor_shut_down(mon);
    mon = NULL;
#endif

    pthread_exit(0);
    return 0;
}

#endif

/* Monitor Implementation Ends here */











/* Assembly line implementation starts here */

/* Fn to be used by wait_queue_t data structure to block all threads
 * until some specified condn is met. This fn is written as per wait-queue_t
 * specification
 * */
static bool
asl_wait_until_all_workers_ready(void *app_arg,
        pthread_mutex_t **locked_appln_mutex) {

    bool rc;
    assembly_line_t *asl = (assembly_line_t *)app_arg;

    if (locked_appln_mutex) {
        *locked_appln_mutex = &asl->mutex;
        pthread_mutex_lock(&asl->mutex);
    }

    rc = (asl->n_workers_ready != asl->asl_size);

    return  rc;
}


/* The slot pointed by First worker thread has to be
 * empty to accomodate a new object in ASL Queue
 */
static void
assembly_line_enqueue_new_item(assembly_line_t *asl,
        void *object) {

    asl_worker_t *T0 = asl->T0;
    assert(asl->asl_q->elem[T0->curr_slot] == 0);
    Fifo_insert_or_replace_at_index(asl->asl_q, object, T0->curr_slot);
}

static asl_worker_t *
assembly_line_create_new_worker_thread (assembly_line_t *asl,
        uint32_t slot_no) {

    char th_name[32];

    asl_worker_t * worker_thread =
        (asl_worker_t *)calloc(1, sizeof(asl_worker_t));

    sprintf(th_name, "thread_%d", slot_no);
    worker_thread->worker_thread = create_thread(0, th_name, THREAD_ANY);
    worker_thread->curr_slot = slot_no;
    worker_thread->asl = asl;
    init_glthread(&worker_thread->worker_thread_glue);
    return worker_thread;
}

static void *
worker_thread_init(void *arg) {

    Fifo_Queue_t *asl_q;
    asl_worker_t *worker_thread = (asl_worker_t *)arg;
    assembly_line_t *asl = worker_thread->asl;

    asl_q = asl->asl_q;

    while(1) {

        if (!worker_thread->initialized) {
            worker_thread->initialized = true;
            worker_thread->work = *(asl->work_fns + worker_thread->curr_slot);
            assert(worker_thread->work);
            pthread_mutex_lock(&asl->mutex);
            asl->n_workers_ready++;
            if (asl->n_workers_ready == asl->asl_size) {
                wait_queue_signal(&asl->asl_ready_wq, false);
            }
            pthread_cond_wait(&worker_thread->worker_thread->cv, &asl->mutex);
            pthread_mutex_unlock(&asl->mutex);
            printf("worker thread %s wakes up\n", worker_thread->worker_thread->name);
        }

        /*  Worker thread must process their respective objects in ASL in parallel,
         *  no Question of locking asl->mutex here as it will serialize the worker
         *  threads operation - What is the point of ASL then !! */

        if (asl_q->elem[worker_thread->curr_slot]) {
#if 0 
            printf("worker thread %s operating on object: \n", 
                    worker_thread->worker_thread->name);
            asl->asl_process_finished_product(asl_q->elem[worker_thread->curr_slot]);
#endif
            /* Execute Worker thread operation on Object now. Note that, this code
             * needs to be run in parallel by all worker threads, Dont mutual exclusate
             * this section of the code*/
            (worker_thread->work)( (void *) (asl_q->elem[worker_thread->curr_slot]));

            if (worker_thread == asl->Tn) {

                printf("worker thread %s was last worker in asl, removing the "
                        "finished object from asl\n",
                        worker_thread->worker_thread->name);

                pthread_mutex_lock(&asl->mutex);
                void *finished_obj = Fifo_insert_or_replace_at_index(
                        asl->asl_q, 0,
                        worker_thread->curr_slot);
                asl->asl_process_finished_product(finished_obj);
                pthread_mutex_unlock(&asl->mutex);
            }
        }

        if (worker_thread->curr_slot == 0) {
            worker_thread->curr_slot = asl->asl_size - 1;
        }
        else {
            worker_thread->curr_slot--;
        }

        pthread_mutex_lock(&asl->mutex);
        asl->n_workers_ready++;
        if (asl->n_workers_ready == asl->asl_size) {
            pthread_cond_signal(&asl->asl_engine_thread->cv);
        }
        pthread_cond_wait(&worker_thread->worker_thread->cv, &asl->mutex);
        pthread_mutex_unlock(&asl->mutex);
    }

    return NULL;
}

static void
assembly_line_start_worker_thread(asl_worker_t *worker_thread) {

    run_thread(worker_thread->worker_thread,
            worker_thread_init,
            (void *)worker_thread);
}

void
assembly_line_init_worker_threads (assembly_line_t *asl) {

    int i;
    asl_worker_t *worker_thread;

    /* A dummy thread data structure created to represent the main
       thread i.e. the thread which is executing 'this' code
       */
    thread_t *main_thread = create_thread(0, "main-thread",  THREAD_ANY);

    for (i = 0 ; i < asl->asl_size; i++) {

        worker_thread = assembly_line_create_new_worker_thread(asl, i);

        glthread_add_next(&asl->worker_threads_head,
                &worker_thread->worker_thread_glue);

        /*  Cache the first worker thread, we will use this info to
         *  insert new object in ASL Queue at T0->curr_slot number */ 
        if (i == 0) {
            asl->T0 = worker_thread;
        }
        /*
         * Cache the last worker thread, we will use this to remove
         * the object from ASL Queue when serviced by last worker thread
         */
        else if (i == asl->asl_size - 1) {
            asl->Tn = worker_thread;
        }

        assembly_line_start_worker_thread(worker_thread);
    }

    wait_queue_test_and_wait(&asl->asl_ready_wq,
            asl_wait_until_all_workers_ready,
            (void *)asl, main_thread);

    printf("All worker threads ready \n");

    free(main_thread);
    main_thread = 0;
}

assembly_line_t *
assembly_line_get_new_assembly_line(char *asl_name,
        uint32_t size,
        void (*asl_process_finished_product)(void *arg)){

    assembly_line_t *asl = (assembly_line_t *) calloc(1, sizeof(assembly_line_t));

    strncpy(asl->asl_name, asl_name, sizeof(asl->asl_name));
    asl->asl_size = size;

    asl->asl_state = ASL_NOT_STARTED;

    /* initialize Circular Queue */
    asl->asl_q =  Fifo_initQ(size, true);

    wait_queue_init(&asl->asl_ready_wq, false, 0);
    asl->n_workers_ready = 0;

    /* initialize Assembly line Mutex */
    pthread_mutex_init(&asl->mutex, NULL);

    /* Thread will be created when item will be pushed into
       ASL */
    asl->asl_engine_thread = NULL;

    asl->asl_process_finished_product = asl_process_finished_product;

    asl->work_fns = (generic_fn_ptr *)calloc(
            asl->asl_size, sizeof(generic_fn_ptr));

    asl->wait_lst_fq = Fifo_initQ(50, false);

    return asl;
}

/* ASL engine function, this fn kick-starts all the ASL worker threads
 * and wait for all of them to complete processing on their respective
 * objects. When all worker threads finished processing, ASL engine is
 * signalled, it dequeues the object from wait list into ASL Queue and
 * kick-starts ASL worker threads fir next cycle of processing
 * */
static void *
asl_engine_fn(void *arg) {

    glthread_t *curr;
    asl_worker_t *worker_thread;
    void *wait_lst_item;

    assembly_line_t *asl = (assembly_line_t *) arg;

    /* ASL asl_q is updated either by ASL engine thread or by
     * ASL worker threads, obey Mutual exclusion */

    pthread_mutex_lock(&asl->mutex);

    do { 

        asl->asl_state = ASL_IN_PROGRESS;
        asl->n_workers_ready = 0;

        /* ASL engine invoking all worker threads again */
        ITERATE_GLTHREAD_BEGIN(&asl->worker_threads_head, curr) {

            worker_thread = worker_thread_glue_to_asl_worker_thread(curr);
            pthread_cond_signal(&worker_thread->worker_thread->cv);
        } ITERATE_GLTHREAD_END(&asl->worker_threads_head, curr)

        while(asl->n_workers_ready != asl->asl_size) {

            pthread_cond_wait(&asl->asl_engine_thread->cv, &asl->mutex);
            continue;
        }

        /* ASL thread is unblocked, asl->mutex is not yet released by WQ */
        wait_lst_item =  Fifo_deque(asl->wait_lst_fq);

        if (wait_lst_item) {
            assembly_line_enqueue_new_item(asl, wait_lst_item);
        }

        if (asl->asl_q->count) {
            continue;
        }
        /* No more objects in ASL Queue pipeline, ASL engine will block
         * itself, and will be woken up when some appln enqueues new
         * object in ASL Queue */
        else {
            asl->asl_state = ASL_WAIT;
            printf("Assembly line came to halt\n");
            pthread_cond_wait(&asl->asl_engine_thread->cv, &asl->mutex);
        }
    } while(1);

    /* Dead code, let us complete the formality */ 
    pthread_mutex_unlock(&asl->mutex);
    return NULL;
}

/* Caller has the responsibility to lock the ASL mutex if required.
 * This ASL engine thread is like a heart beat of ASL Queue, it re-starts
 * the ASL Queue processing when appln pushes new objects in ASL Queue
 * */
static void
assembly_line_engine_start(assembly_line_t *asl) {

    if (!asl->asl_engine_thread) {

        printf("Creating Assembly line engine thread\n");

        asl->asl_engine_thread = 
            create_thread (0, "assembly_line_engine_thread", THREAD_ANY);

        run_thread(asl->asl_engine_thread,
                asl_engine_fn,
                (void *)asl);
    }
    else if (asl->asl_state == ASL_WAIT){
        asl->asl_state = ASL_IN_PROGRESS;
        printf(" Assembly line engine signalled\n");
        pthread_cond_signal(&asl->asl_engine_thread->cv);
    }
}

/*
 * Used by the Appns (clients of ASL) to push the new objects to be
 * passed through ASL processing. Appln could be multithreaded, so this
 * API could be invoked by multiple application threads
 */
void
assembly_line_push_new_item(assembly_line_t *asl,
        void *new_item) {

    glthread_t *curr;
    asl_worker_t *worker_thread;

    /* lock the ASL, since we are going to update the ASL Queue */
    pthread_mutex_lock(&asl->mutex);

    /* Push the first item in the assembly line */
    if (!asl->asl_q->count) {
        assembly_line_enqueue_new_item(asl, new_item); 
    }
    /* It means, there are some objects in the ASL Queue in process,
     * let us Queue the new arrivals in backup Queue. Whenever ASL makes
     * a progress, ASL will pick up one object from WQ and push into start
     * of the ASL Queue. Let's mimic our solution to real world as close as
     * possible */
    else if (!is_queue_full(asl->wait_lst_fq)) {
        Fifo_enqueue(asl->wait_lst_fq, new_item);
    }
    else {
        assert(0);
    }

    /* Start the ASL engine thread since we some some objects to process.
     * ASL engine is started only once, repeated calls to this API would be
     * no-op
     * */
    assembly_line_engine_start(asl);
    pthread_mutex_unlock(&asl->mutex);
}

void
assembly_line_register_worker_fns(assembly_line_t *asl,
        uint32_t slot,
        generic_fn_ptr work_fn){

    asl->work_fns[slot] = work_fn;
}


#if 1

typedef struct car_{

    char name[32];
    uint32_t flags;
} car_t;

void
appln_asl_process_finished_product_fn(void *object) {

    car_t *car = (car_t *)object;
    printf("Finished Object : car->name = %s car->flags = %u\n",
            car->name, car->flags);
}

#define CAR_F_WHITE_PAINT   1
#define CAR_F_WHEELS        2
#define CAR_F_ENGINE        4
#define CAR_F_WINDOWS       8
#define CAR_F_FINISHING     16

void *
car_add_white_paint(void *object) {

    car_t *car = (car_t *)object;
    car->flags |= CAR_F_WHITE_PAINT;
    printf("ASL Update : Car %s : Paint Added\n", car->name);
    return 0;
}

void *
car_add_wheels(void *object) {

    car_t *car = (car_t *)object;
    car->flags |= CAR_F_WHEELS;
    printf("ASL Update : Car %s : Wheels Added\n", car->name);
    return 0;
}

void *
car_add_engine(void *object) {

    car_t *car = (car_t *)object;
    car->flags |= CAR_F_ENGINE;
    printf("ASL Update : Car %s : Engine Added\n", car->name);
    return 0;
}

void *
car_add_windows(void *object) {

    car_t *car = (car_t *)object;
    car->flags |= CAR_F_WINDOWS;
    printf("ASL Update : Car %s : Windows Added\n", car->name);
    return 0;
}

void *
car_add_finishing(void *object) {

    car_t *car = (car_t *)object;
    car->flags |= CAR_F_FINISHING;
    printf("ASL Update : Car %s : Finishing Added\n", car->name);
    return 0;
}

int
main(int argc, char **argv) {

    /*  Create an assembly line */
    assembly_line_t *asl = assembly_line_get_new_assembly_line(
            "Car-Manufacturing Assembly Line",
            5,
            appln_asl_process_finished_product_fn);

    /*  Register worker functions for each slot */
    assembly_line_register_worker_fns(asl, 0, car_add_white_paint);
    assembly_line_register_worker_fns(asl, 1, car_add_wheels);
    assembly_line_register_worker_fns(asl, 2, car_add_engine);
    assembly_line_register_worker_fns(asl, 3, car_add_windows);
    assembly_line_register_worker_fns(asl, 4, car_add_finishing);

    /* intitialize worker threads */
    assembly_line_init_worker_threads(asl);

    /* Now you can push objects to assembly line pipeline */
    car_t *car1, *car2, *car3, *car4;

    car1 = calloc(1, sizeof(car_t));
    strcpy(car1->name, "indica");

    car2 = calloc(1, sizeof(car_t));
    strcpy(car2->name, "etios");

    car3 = calloc(1, sizeof(car_t));
    strcpy(car3->name, "toyota");

    car4 = calloc(1, sizeof(car_t));
    strcpy(car4->name, "BMW");

    while(1) {
        assembly_line_push_new_item(asl, (void *)car1);
        assembly_line_push_new_item(asl, (void *)car2);
        assembly_line_push_new_item(asl, (void *)car3);
        assembly_line_push_new_item(asl, (void *)car4);
        sleep(1);
        car1->flags = 0;
        car2->flags = 0;
        car3->flags = 0;
        car4->flags = 0;
    }
    pthread_exit(0);
    return 0;
}

#endif

/* Assembly line implementation Ends here */


/*
Visit : www.csepracticals.com for more courses and projects
Join Telegram Grp : telecsepracticals
*/




