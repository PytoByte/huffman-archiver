#include "minheap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HuffmanNode* HuffmanNode_create(
    uint8_t wordsize,
    uint8_t* word,
    unsigned long long freq,
    HuffmanNode* left,
    HuffmanNode* right
) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (node == NULL) return NULL;
    if (word == NULL) {
        node->word = NULL;
        node->wordsize = 0;
        if (wordsize != 0) {
            fprintf(stderr, "WARNING: In HuffmanNode_create() word NULL with size > 0\n");
        }
    } else {
        node->wordsize = wordsize;
        int wordsize_bytes = (wordsize/8 + (wordsize%8 > 0));
        node->word = (unsigned char*)malloc(sizeof(unsigned char) * wordsize_bytes);
        if (!node->word) return NULL;
        strncpy(node->word, word, wordsize_bytes);
    }
    node->freq = freq;
    node->left = left;
    node->right = right;
    return node;
}

// Начало === Поддержание свойств кучи ===
static void swap(HuffmanNode** a, HuffmanNode** b) {
    HuffmanNode* temp = *a;
    *a = *b;
    *b = temp;
}

static unsigned char parent(unsigned char i) {
    return (i - 1) / 2;
}

static unsigned char child_left(unsigned char i) {
    return i * 2 + 1;
}

static unsigned char child_right(unsigned char i) {
    return i * 2 + 2;
}

static void heapify_up(MinHeap* heap, unsigned char i) {
    HuffmanNode** nodes = heap->nodes;

    while (1) {
        if (i==0) return;
    
        if (nodes[parent(i)]->freq > nodes[i]->freq) {
            swap(&nodes[parent(i)], &nodes[i]);
            i = parent(i);
        } else return;
    }
}

static void heapify_down(MinHeap* heap, unsigned char i) {
    HuffmanNode** nodes = heap->nodes;

    while (1) {
        if (child_left(i)<=i || child_right(i)<=i ||
            child_left(i)>=heap->size || child_right(i)>=heap->size
        ) return;

        if (nodes[child_left(i)]->freq < nodes[i]->freq) {
            swap(&nodes[child_left(i)], &nodes[i]);
            i = child_left(i);
        } else if (nodes[child_right(i)]->freq < nodes[i]->freq) {
            swap(&nodes[child_right(i)], &nodes[i]);
            i = child_right(i);
        } else return;
    }
}
// Конец === Поддержание свойств кучи ===

static void insert(MinHeap* self, HuffmanNode* node) {
    self->nodes[self->size] = node;
    heapify_up(self, self->size++);
}

static HuffmanNode* extract(MinHeap* self) {
    if (self->size==0) {
        return NULL;
    }
    HuffmanNode* res = self->nodes[0];
    swap(&self->nodes[0], &self->nodes[--self->size]);
    heapify_down(self, 0);
    return res;
}

// Извлечение всех узлов в формате бинарного дерева
static HuffmanNode* extract_tree(MinHeap* self) {
    HuffmanNode* tree;

    while (self->size > 1) {
        HuffmanNode* node1 = extract(self);
        HuffmanNode* node2 = extract(self);

        // Создание детерминированности. Среди двух минимальных по частоте узлов, правее = больше частота
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

MinHeap* MinHeap_create(int capacity) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    if (heap == NULL) return NULL;
    heap->size = 0;
    heap->capacity = capacity;
    heap->nodes = (HuffmanNode**)malloc(sizeof(HuffmanNode*) * capacity);
    heap->insert = insert;
    heap->extract = extract;
    heap->extract_tree = extract_tree;
    return heap;
}

void MinHeap_free(MinHeap* heap) {
    for (int i = 0; i < heap->size; i++) {
        free(heap->nodes[i]);
    }
    free(heap->nodes);
    free(heap);
}

void HuffmanNode_freetree(HuffmanNode* tree) {
    if (tree->left) HuffmanNode_freetree(tree->left);
    if (tree->right) HuffmanNode_freetree(tree->right);
    if (tree->word) free(tree->word);
    free(tree);
}