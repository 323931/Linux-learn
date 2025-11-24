#include "thrd_pool.h"
#include <pthread.h>
#include <atomic>
#include <queue>

typedef struct task_s{
    void* next;
    handler_pt handler;
    void* arg;
}task_t;

typedef struct task_queue_s{
    std::queue<task_t> tasks;
    void *head;
    void **tail;
    int block;
    pthread_spinlock_t lock;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}task_queue_t;

struct thrdpool_s{
    task_queue_t *task_queue;
    std::atomic<int> quit;
    int thrd_numl;
    pthread_t *threads;
};


