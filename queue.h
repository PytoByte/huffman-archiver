typedef struct QueueElem {
    struct QueueElem *next;
    void *value;
} QueueElem;

typedef struct {
    QueueElem *first;
    QueueElem *last;
} Queue;

// Returns NULL if unsuccessful
Queue *queue_create();

// Returns 0 if successful, 1 otherwise
int queue_enqueue(Queue *queue, void *value);

// Returns NULL if the queue is empty
void* queue_dequeue(Queue *queue);

// Frees all the elements in the queue and set its pointer to NULL
void queue_destroy(Queue **queue, void (*freedata) (void*));