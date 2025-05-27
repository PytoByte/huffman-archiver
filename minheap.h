#pragma once

typedef struct HuffmanNode {
    unsigned char byte;
    unsigned long long freq;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

typedef struct {
    HuffmanNode* tree;
    int node_count;
    int leaf_count;
} HuffmanTree;

HuffmanNode* HuffmanNode_create(
    unsigned char byte,
    unsigned long long freq,
    HuffmanNode* left,
    HuffmanNode* right
);

typedef struct {
    short size;
    HuffmanNode* nodes[256];
} MinHeap;

MinHeap* MinHeap_create();

void MinHeap_insert(MinHeap* heap, HuffmanNode* node);

HuffmanNode* MinHeap_extract(MinHeap* heap);

// Извлечение всех узлов в формате бинарного дерева
HuffmanTree MinHeap_extract_tree(MinHeap* heap);