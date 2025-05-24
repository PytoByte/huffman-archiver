#pragma once

#include <stdlib.h>
#include <limits.h>

typedef struct HuffmanNode {
    unsigned char byte;
    unsigned long long freq;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

HuffmanNode* HuffmanNode_create(
    unsigned char byte,
    unsigned long long freq,
    HuffmanNode* left,
    HuffmanNode* right
);

typedef struct {
    short size;
    HuffmanNode* nodes[UCHAR_MAX+1];
} MinHeap;

MinHeap* MinHeap_create();

void MinHeap_insert(MinHeap* heap, HuffmanNode* node);

HuffmanNode* MinHeap_extract(MinHeap* heap);

// Извлечение всех узлов в формате бинарного дерева
HuffmanNode* MinHeap_extract_tree(MinHeap* heap);