/*
 * =====================================================================================
 *
 *       Filename:  hello_world.c
 *
 *    Description: This file demonstrates the use of POSIX threads - A hello world program 
 *
 *
 * =====================================================================================
 */

/*
 * compile using :
 * gcc -g -c hello_world.c -o hello_world.o
 * gcc -g hello_world.o -o hello_world.exe -lpthread
 * Run : ./hello_world.exe
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> /* For working with POSIX threads*/
#include <unistd.h>  /* For pause() and sleep() */
#include <errno.h>	 /* For using Global variable errno */

/* A thread callback fn must have following prototypes 
 * void *(*thread_fn)(void *)
 * */

static void* thread_fn_callback(void*arg){
	char* input=(char*)arg;
	while(1){
		printf( " the input string is : %s\n",input);
		sleep(1);
	}
}
void thread_create(){

	pthread_t thread1;
	static char *threadinput1 = "hello im samrat " ;
	int rc = pthread_create (&thread1, NULL, thread_fn_callback, (void*)threadinput1);
	if(rc!=0){
		printf("error\n");
	}

}




int main (int argc, char**argv){
	thread_create();
	// now pause 
	printf("main thread is paused \n ");
	while(1){
		printf("main thread is runing... \n");
		sleep(1);
	}
	pause(); 
  return 0 ;

}
