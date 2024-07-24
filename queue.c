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

void add(struct lnkList* lst, struct Node* vertex) {
    if(lst->head == NULL) {
        lst->tail = lst->head = vertex;
    }
    else {
        lst->tail->nxt = vertex;
        lst->tail = lst->tail->nxt;
    }
}
void* pop(struct lnkList* lst) {
    void* result = lst->head->item;
    struct Node* tmp = lst->head;
    lst->head = tmp->nxt;
    free(tmp);
    if(lst->head == NULL) {
        lst->tail = NULL;
    }
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
    //cvList->head = cvList->sentinel;
    //cvList->tail = cvList->sentinel;
    tasks = (struct lnkList*)calloc(sizeof(struct lnkList),1);
    tasks->sentinel = (struct Node*)calloc(sizeof(struct Node),1);
    //tasks->head = tasks->sentinel;
    //tasks->tail = tasks->sentinel;
    dqConut = 0;
    done = 0;
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
    if(curr != NULL) {
        cnd_destroy(&curr->cv);
        pop(cvList);
    }


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
    if (tasks->sz > 0 && dqConut > 0) {
        cnd_signal(&cvList->head->cv);
    }
    mtx_unlock(&queueLock);
    done++;
    *val = result;
    return true;
}

void destroyQueue() {
    struct Node* last = tasks->head;
    struct Node* curr = last ? last->nxt : NULL;
    while(last != NULL) {
        free(last);
        last = curr;
        curr = last ? last->nxt : NULL;
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
