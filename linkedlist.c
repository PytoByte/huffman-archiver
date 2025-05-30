#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linkedlist.h"

hlist* initlist() {
    hlist* head = (hlist*)malloc(sizeof(hlist));
    head->begin = NULL;
    return head;
}

void addtolist(hlist* head, char* path, long long size_pos) {
    llist* new = (llist*)malloc(sizeof(llist));
    new->next = head->begin;
    new->path = (char*)malloc(strlen(path)+1);
    strcpy(new->path, path);
    new->size_pos = size_pos;

    head->begin = new;
}

void freelist(hlist* head) {
    llist* cur = head->begin;
    while (cur != NULL) {
        llist* next = cur->next;
        free(cur->path);
        free(cur);
        cur = next;
    }
    free(head);
}