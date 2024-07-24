#include <threads.h>
#include "queue.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>

void print_result(const char *test_name, bool result) {
    printf("%s: %s\n", test_name, result ? "PASSED" : "FAILED");
}

static int numOfEnq;
static int numOfDeq;

struct Node {
    void* item;
    thrd_t id;
    cnd_t cv;
    int numOfWaits;
    struct Node* nxt;
};

struct lnkList {
    struct Node* sentinel;
    struct Node* head;
    struct Node* tail;
    int sz; //
};

void add(struct lnkList* lst, struct Node* vertex) {
    if(lst->head == NULL) {
        lst->head = vertex;
        lst->tail = vertex;
    }
    else {
        lst->tail->nxt = vertex;
        lst->tail = lst->tail->nxt;
    }
}
void* pop(struct lnkList* lst) {
    if(lst->head == NULL) {
        return NULL;
    }
    void* result = lst->head->item;
    struct Node* tmp = lst->head;
    lst->head = tmp->nxt;
    free(tmp);
    if(lst->head == NULL) {
        lst->tail = NULL;
    }
    lst->sentinel->nxt = lst->head;
    return result;
}

struct lnkList* tasks;
struct lnkList* cvList;
atomic_int dqConut; //number of threads waiting for tasks, not for lock!
atomic_int done;

mtx_t queueLock;

void initQueue() {
    mtx_init(&queueLock, mtx_plain);
    cvList = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    cvList->sentinel = (struct Node*)calloc(sizeof(struct Node),1);
    tasks = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    tasks->sentinel = (struct Node*)calloc(sizeof(struct Node),1);

    dqConut = 0;
    done = 0;
}

void enqueue(void* data) {
    mtx_lock(&queueLock);
    //printf("Task now\n");
    //printf("Number of waiting threads : %d\n",dqConut);
    //printf("Number of elements in tasks : %d\n",tasks->sz);
    struct Node* element = (struct Node*)calloc(sizeof(struct Node),1);
    element->item = data;
    element->nxt = NULL;
    add(tasks,element);
    tasks->sz++;
    if(cvList->head != NULL) {
        //("Number of waiters at first cv : %d\n", cvList->head->numOfWaits);
        cnd_signal(&cvList->head->cv);
    }
    mtx_unlock(&queueLock);
}

void* dequeue() {
    mtx_lock(&queueLock);
    //printf("Thread now\n");
    //printf("Number of waiting threads : %d\n",dqConut);
    //printf("Number of elements in tasks : %d\n",tasks->sz);
    if(tasks->sz - dqConut <= 0) { //aka we wait for new task.
        cnd_t cv;
        cnd_init(&cv);
        struct Node* element = (struct Node*)calloc(sizeof(struct Node),1);
        element->cv = cv;
        element->nxt = NULL;
        element->numOfWaits = 0;
        add(cvList,element);
    }
    dqConut++;
    struct Node* curr = cvList->tail;

    if(dqConut > 1 || tasks->sz == 0) {
        curr->numOfWaits++;
        cnd_wait(&cvList->tail->cv,&queueLock);
        //printf("Sanity Sanity Sanity\n");
        curr->numOfWaits--;
    }

    void* result = pop(tasks);
    tasks->sz--;
    dqConut--;
    //printf("Tasks' size : %d , dqCount : %d\n",tasks->sz,dqConut);
    if(tasks->sz > 0 && dqConut > 0) { //aka somebody is sleeping and waiting for us - NEED TO BE SUPER CAREFUL waiter m8 be in our cv, wasted 3 hours
        if(curr->numOfWaits > 0) {
            cnd_signal(&curr->cv);
        }
        else {
            cnd_signal(&curr->nxt->cv);
        }
        //printf("Mood Mood Mood Mood Mood Mood\n");
    }
    if(cvList->head != NULL && cvList->head->numOfWaits == 0) { //Nobody is in our cv anymore, time to erase it
        //printf("Number of waiters : %d\n", cvList->head->numOfWaits);
        cnd_destroy(&cvList->head->cv);
        pop(cvList);
    }

    //printf("Num of dnq : %d\n",numOfDeq);
    mtx_unlock(&queueLock);
    done++;
    return result;
}

bool tryDequeue(void **val) {
    if (mtx_trylock(&queueLock) != thrd_success) {
        return false;
    }
    if (tasks->sz - dqConut <= 0) {
        mtx_unlock(&queueLock);
        return false;
    }
    dqConut++;
    void* result = pop(tasks);
    tasks->sz--;
    dqConut--;
    if (tasks->sz > 0 && dqConut > 0 && cvList->head != NULL) {
        cnd_signal(&cvList->head->cv);
    }
    mtx_unlock(&queueLock);
    done++;
    *val = result;
    return true;
}

void destroyQueue() { //Mostly memory stuff
    struct Node* last = tasks->head;
    struct Node* curr = last ? last->nxt : NULL;
    while(last != NULL) {
        free(last);
        last = curr;
        curr = last ? last->nxt : NULL;
    }
    if(tasks != NULL) {
        free(tasks->sentinel);
    }
    free(tasks);

    last = cvList->head;
    curr = last ? last->nxt : NULL;
    while(last != NULL) {
        cnd_destroy(&last->cv);
        free(last);
        last = curr;
        curr = last ? last->nxt : NULL;
    }
    free(cvList->sentinel);
    free(cvList);

    mtx_destroy(&queueLock);
}

size_t size(void) {
    return tasks->sz;
}
size_t waiting(void) {
    return dqConut;
}
size_t visited(void) {
    return done;
}
