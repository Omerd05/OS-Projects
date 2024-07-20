#include <threads.h>
#include "queue.h"
#include <stdlib.h>
#include <stdatomic.h>

struct Node {
    void* item;
    thrd_t id;
    cnd_t cv;
    struct Node* nxt;
};

struct lnkList {
    struct Node* sentinel;
    struct Node* head;
    struct Node* tail;
    size_t sz;
};

void add(struct lnkList* lst, struct Node*) {

}
void* pop(struct lnkList* lst) {

}

struct lnkList* tasks;
struct lnkList* cvList;
atomic_int dqConut; //Waiting for tasks, not for lock!


mtx_t queueLock;

/*void* getItem() { //returns with matching enqueued item
    struct Node* curr = (struct Node*)calloc(sizeof(struct Node),1);
    curr->id = thrd_current();
    curr->nxt = NULL;
    struct Node* prev = thrSlep->tail;
    add(thrSlep,curr);
    thrd_join(prev->id,NULL);
    void* result = NULL;
    if(tasks->sz == 0) {
        cnd_wait(&idle,NULL);
    }
    return pop(tasks);
}*/

void initQueue() {
    mtx_init(&queueLock, mtx_plain);
    cvList = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    (struct Node*)calloc(sizeof(struct Node),1);

    thrd_exit(EXIT_SUCCESS);

    /*qData = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    qData->sentinel = (struct Node*)calloc(sizeof(struct Node),1);
    thrSupps = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    thrSupps->sentinel = (struct Node*)calloc(sizeof(struct Node),1);

    mtx_init(&inputLock,mtx_plain);
    mtx_init(&suppLock,mtx_plain);*/
    //thrd_exit(EXIT_SUCCESS);
}

void enqueue(void* data) {
    mtx_lock(&queueLock);
    struct Node* element = (struct Node*)calloc(sizeof(struct Node),1);
    element->item = data;
    element->nxt = NULL;
    add(tasks,element);
    tasks->sz++;
    if(dqConut > 0) {
        cnd_signal(&cvList->head->cv);
    }
    mtx_unlock(&queueLock);
}

void* dequeue(void) {
    mtx_lock(&queueLock);

    if(tasks->sz - dqConut <= 0) { //aka we wait for new task.
        cnd_t cv;
        cnd_init(&cv);
        struct Node* element = (struct Node*)calloc(sizeof(struct Node),1);
        element->cv = cv;
        element->nxt = NULL;
        add(cvList,element);
        //cvList->sz++;
    }
    dqConut++;
    struct Node* curr = cvList->tail;
    if(dqConut > 1 || tasks->sz == 0) {
        cnd_wait(&cvList->tail->cv,&queueLock);
    }
    void* result = pop(tasks);
    tasks->sz--;
    dqConut--;
    if(tasks->sz > 0 && dqConut > 0) { //aka somebody is sleeping and waiting for us
        cnd_signal(&curr->nxt->cv);
    }
    mtx_unlock(&queueLock);
    return result;
    //WARNING WARNING WARNING - now that we've unlocked another thread may print before us even if we were first.*/
}

void destroyQueue() {
    struct Node* last = tasks->sentinel;
    struct Node* curr = last->nxt;
    while(last != NULL) {
        free(last);
        last = curr;
    }
    free(tasks);
    last = cvList->sentinel;
    curr = last->nxt;
    while(last != NULL) {
        free(last);
        last = curr;
    }
    free(cvList);
}

