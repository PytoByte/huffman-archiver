#pragma once

#include <stdint.h>

typedef struct HuffmanNode {
    uint8_t wordsize;
    uint8_t* word;
    unsigned long long freq;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

HuffmanNode* HuffmanNode_create(
    uint8_t wordsize,
    uint8_t* word,
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