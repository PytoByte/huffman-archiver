typedef struct QueueElem {
    struct QueueElem *next;
    void *value;
} QueueElem;

typedef struct {
    QueueElem *first;
    QueueElem *last;
} Queue;

Queue *queue_create();

void queue_enqueue(Queue *queue, void *value);

void* queue_dequeue(Queue *queue);

void queue_destroy(Queue **queue, void (*freedata) (void*));