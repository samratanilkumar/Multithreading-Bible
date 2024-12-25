#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "rw_locks.h"

static rw_lock_t rw_lock;
static int n_r =  0;
static int n_w = 0;
pthread_mutex_t state_check_mutex;


static void
cs_status_check() {

    pthread_mutex_lock(&state_check_mutex);
    assert(n_r >= 0 && n_w >= 0); /* Cannot be negative */

    if (n_r == 0 && n_w == 0) {
        // valid condition
    }
    else if (n_r > 0 && n_w == 0) {
        // valid condition
    }
    else if (n_r == 0 && n_w == 1) {
        // valid condition
    }
    else 
        assert(0);

    printf ("n_r = %d, n_w = %d\n", n_r, n_w);
    pthread_mutex_unlock(&state_check_mutex);
}

static void 
execute_cs() {

    cs_status_check();
}


static void
reader_enter_cs () {

    pthread_mutex_lock(&state_check_mutex);
        n_r++;
    pthread_mutex_unlock(&state_check_mutex);
}

static void
reader_leave_cs () {

    pthread_mutex_lock(&state_check_mutex);
        n_r--;
    pthread_mutex_unlock(&state_check_mutex);
}

static void
writer_enter_cs () {
        n_w++;
}

static void
writer_leave_cs () {
        n_w--;
}

void *
read_thread_fn (void *arg) {

    while(1) {
        rw_lock_rd_lock(&rw_lock);
        reader_enter_cs();
       
       execute_cs();

        reader_leave_cs();
        rw_lock_unlock(&rw_lock);
    }
}

void *
write_thread_fn (void *arg) {

    while(1) {
        rw_lock_wr_lock(&rw_lock);
        writer_enter_cs();
        
       execute_cs();

        writer_leave_cs();
        rw_lock_unlock(&rw_lock);
    }
}

int
main(int argc, char **argv) {

   static pthread_t th1, th2, th3, th4, th5, th6;
    rw_lock_init(&rw_lock);
    pthread_mutex_init (&state_check_mutex, NULL);
    pthread_create(&th1, NULL, read_thread_fn, NULL);
    pthread_create(&th2, NULL, read_thread_fn, NULL);
    pthread_create(&th3, NULL, read_thread_fn, NULL);
    pthread_create(&th4, NULL, write_thread_fn, NULL);
    pthread_create(&th5, NULL, write_thread_fn, NULL);
    pthread_create(&th6, NULL, write_thread_fn, NULL);
    pthread_exit(0);
    return 0;
}