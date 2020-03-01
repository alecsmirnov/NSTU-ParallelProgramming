#include "condvarimitation.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define PTHREAD_PASS 0

#define pthreadErrorHandle(test_code, pass_code, err_msg) do {		\
	if (test_code != pass_code) {									\
		fprintf(stderr, "%s: %s\n", err_msg, strerror(test_code));	\
		exit(EXIT_FAILURE);											\
	}																\
} while (0)

#define signalToLock(list) do {             \
    bool* lock = *(bool**)listFront(list);  \
    *lock = false;                          \
    listPopFront(list);                     \
} while (0);

static void StateListFree(void* arg) {
    bool* lock = *(bool**)arg;
    *lock = false;
}

void pthreadCondInit(CondType* cond) {
    int err = pthread_mutex_init(&cond->mutex, NULL);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot initialize mutex");

    listInit(&cond->state_list, sizeof(bool**), StateListFree);
}

void pthreadCondWait(CondType* cond, pthread_mutex_t* mutex) {
    int err = pthread_mutex_lock(&cond->mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot lock condition mutex");

    bool* lock = (bool*)malloc(sizeof(bool));
    *lock = true;
    listPushBack(cond->state_list, &lock);

    err = pthread_mutex_unlock(&cond->mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot unlock condition mutex");

    err = pthread_mutex_unlock(mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot unlock mutex");

    while (*lock)
        usleep(WAIT_TIME);

    err = pthread_mutex_lock(mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot lock mutex");
    free(lock);
}

void pthreadCondSignal(CondType* cond) {
    int err = pthread_mutex_lock(&cond->mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot lock condition mutex");

    if (!listIsEmpty(cond->state_list)) 
        signalToLock(cond->state_list);

    err = pthread_mutex_unlock(&cond->mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot unlock condition mutex");
}

void pthreadCondBroadcast(CondType* cond) {
    int err = pthread_mutex_lock(&cond->mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot lock condition mutex");

    while (!listIsEmpty(cond->state_list)) 
        signalToLock(cond->state_list);

    err = pthread_mutex_unlock(&cond->mutex);
    pthreadErrorHandle(err, PTHREAD_PASS, "Cannot unlock condition mutex");
}

void pthreadCondDestroy(CondType* cond) {
    pthread_mutex_destroy(&cond->mutex);
    listFree(&cond->state_list);
}