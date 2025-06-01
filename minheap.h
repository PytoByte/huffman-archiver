#pragma once

typedef struct HuffmanNode {
    unsigned char wordsize;
    unsigned char* word;
    unsigned long long freq;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

HuffmanNode* HuffmanNode_create(
    unsigned char wordsize,
    unsigned char* word,
    unsigned long long freq,
    HuffmanNode* left,
    HuffmanNode* right
);

typedef struct MinHeap {
    int size;
    int capacity;
    HuffmanNode** nodes;
    void (*insert)(struct MinHeap* self, HuffmanNode* node);
    HuffmanNode* (*extract)(struct MinHeap* self);
    HuffmanNode* (*extract_tree)(struct MinHeap* self);
} MinHeap;

MinHeap* MinHeap_create(int capacity);

void MinHeap_free(MinHeap* heap);

void HuffmanNode_freetree(HuffmanNode* tree);