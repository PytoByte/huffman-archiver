#include "queue.h"

#include <stdlib.h>

Queue *queue_create() {
	Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->first = NULL;
    queue->last = NULL;
    return queue;
}

void queue_enqueue(Queue *queue, void *value) {
	QueueElem* new = (QueueElem*)malloc(sizeof(QueueElem));
    new->value = value;
    new->next = NULL;

    if (queue->last == NULL) {
        queue->first = new;
        queue->last = new;
    } else {
        queue->last->next = new;
    }
}

void* queue_dequeue(Queue *queue) {
    if (queue->first == NULL) return NULL;

	QueueElem* first = queue->first;
	queue->first = first->next;
	void* value = first->value;
	free(first);

    if (queue->first == NULL) queue->last = NULL;

	return value;
}

void queue_destroy(Queue **queue, void (*freedata) (void*)) {
	QueueElem* cur = (*queue)->first;

    while (cur != NULL) {
        QueueElem* next = cur->next;
        if (freedata) freedata(cur->value);
        free(cur);
        cur = next;
    }

    free(*queue);
    *queue = NULL;
}