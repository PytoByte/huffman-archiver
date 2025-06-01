#pragma once

typedef struct llist {
    struct llist* next;
    char* path;
    long long size_pos;
} llist;

typedef struct {
    llist* begin;
} hlist;

hlist* initlist();

void addtolist(hlist* head, char* path, long long size_pos);

void freelist(hlist* head);