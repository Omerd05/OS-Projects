#include <threads.h>
#include "queue.h"
#include <stdlib.h>

struct Node {
    void* item;
    thrd_t id;
    struct Node* nxt;
};

struct lnkList {
    struct Node* sentinel;
    struct Node* head;
    struct Node* tail;
    size_t sz = 0;
};

void add(struct lnkList* lst, struct Node*) {

}
void pop(struct lnkList* lst) {

}


struct lnkList* qData;
struct lnkList* thrSupps;


mtx_t inputLock;
mtx_t suppLock;

cnd_t qInput;

void supporter() {
    struct Node* element = (struct Node*)calloc(sizeof(struct Node),1);
    element->id = thrd_current();
    element->nxt = NULL;
    add(thrSupps,element);
}

void initQueue() {
    qData = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    qData->sentinel = (struct Node*)calloc(sizeof(struct Node),1);
    thrSupps = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    thrSupps->sentinel = (struct Node*)calloc(sizeof(struct Node),1);

    mtx_init(&inputLock,mtx_plain);
    mtx_init(&suppLock,mtx_plain);
    //thrd_exit(EXIT_SUCCESS);
}

void enqueue(void* data) {
    mtx_lock(&inputLock);

    cnd_signal(&qInput);
    mtx_unlock(&inputLock);
}

void* dequeue(void) {

    //mtx_lock(&thrLock);
    if(thrSupps->sz - qData->sz > 0) {

        thrd_t agent;
        mtx_lock(&suppLock);
        thrd_create(agent,supporter,NULL);
        mtx_lock(&suppLock);
        thrd_join(agent,NULL);

        /*struct Node* element = (struct Node*)calloc(sizeof(struct Node),1);
        element->id = thrd_current();
        element->nxt = NULL;
        add(thrSleep,element);
        if(thrSleep>0) {
            thrd_join(thrSleep->head->id,NULL);
        }
        else {
            cnd_wait(&qInput, &thrLock);
        }*/
    }

    void* result = qData->head->item;
    pop(qData);
    //mtx_unlock(&thrLock);
    return result;
}

void destroyQueue() {
    struct Node* last = vals->sentinel;
    struct Node* curr = last->nxt;
    while(last != NULL) {
        free(last);
        last = curr;
    }
    free(vals);
    last = thrs->sentinel;
    curr = last->nxt;
    while(last != NULL) {
        free(last);
        last = curr;
    }
    free(thrs);
}
