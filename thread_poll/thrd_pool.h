typedef struct thrdpool_s thrdpool_t;
typedef void (*handler_pt) (void *arg);

thrdpool_t *thrdpool_create(int thread_num);

void thrdpool_destroy(thrdpool_t *pool);

int thrdpool_add_work(thrdpool_t *pool, handler_pt handler, void *arg);

int thrdpool_wait_done(thrdpool_t *pool);

int thrdpool_get_thread_count(thrdpool_t *pool);

int thrdpool_get_queue_size(thrdpool_t *pool);

int thrdpool_get_active_threads(thrdpool_t *pool);