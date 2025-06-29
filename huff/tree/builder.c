#include "builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// === Keeping heap properties ===
static void swap(HuffmanNode** a, HuffmanNode** b) {
    HuffmanNode* temp = *a;
    *a = *b;
    *b = temp;
}

static unsigned int parent(unsigned int i) {
    return (i - 1) / 2;
}

static unsigned int child_left(unsigned int i) {
    return i * 2 + 1;
}

static unsigned int child_right(unsigned int i) {
    return i * 2 + 2;
}

static void heapify_up(TreeBuilder* heap, unsigned int i) {
    HuffmanNode** nodes = heap->nodes;

    while (1) {
        if (i==0) return;
    
        if (nodes[parent(i)]->freq > nodes[i]->freq) {
            swap(&nodes[parent(i)], &nodes[i]);
            i = parent(i);
        } else return;
    }
}

static void heapify_down(TreeBuilder* heap, unsigned int i) {
    HuffmanNode** nodes = heap->nodes;

    while (1) {
        if (child_left(i)>=heap->size || child_right(i)>=heap->size)
            return;

        if (nodes[child_left(i)]->freq < nodes[i]->freq) {
            swap(&nodes[child_left(i)], &nodes[i]);
            i = child_left(i);
        } else if (nodes[child_right(i)]->freq < nodes[i]->freq) {
            swap(&nodes[child_right(i)], &nodes[i]);
            i = child_right(i);
        } else return;
    }
}
// === Keeping heap properties ===

static void insert(TreeBuilder* self, HuffmanNode* node) {
    self->nodes[self->size] = node;
    heapify_up(self, self->size++);
}

static HuffmanNode* extract(TreeBuilder* self) {
    if (self->size==0) {
        return NULL;
    }
    HuffmanNode* res = self->nodes[0];
    swap(&self->nodes[0], &self->nodes[--self->size]);
    heapify_down(self, 0);
    return res;
}

// Extract all nodes in binary tree format
static HuffmanNode* extract_tree(TreeBuilder* self) {
    HuffmanNode* tree;

    while (self->size > 1) {
        HuffmanNode* node1 = extract(self);
        HuffmanNode* node2 = extract(self);

        // Creating determinism. Among the two nodes with the lowest frequency, the one on the right = higher frequency
        HuffmanNode* merge_node = NULL;
        if (node1->freq >= node2->freq) {
            merge_node = HuffmanNode_create(0, NULL, node1->freq+node2->freq, node2, node1);
        } else {
            merge_node = HuffmanNode_create(0, NULL, node1->freq+node2->freq, node1, node2);
        }

        insert(self, merge_node);
    }

    tree = extract(self);

    return tree;
}


TreeBuilder* TreeBuilder_create(unsigned int capacity) {
    TreeBuilder* heap = (TreeBuilder*)malloc(sizeof(TreeBuilder));
    if (heap == NULL) return NULL;
    heap->size = 0;
    heap->capacity = capacity;
    heap->nodes = (HuffmanNode**)malloc(sizeof(HuffmanNode*) * capacity);
    heap->insert = insert;
    heap->extract_tree = extract_tree;
    return heap;
}


void TreeBuilder_free(TreeBuilder* tb) {
    for (unsigned int i = 0; i < tb->size; i++) {
        HuffmanNode_freetree(tb->nodes[i]);
    }
    free(tb->nodes);
    free(tb);
}