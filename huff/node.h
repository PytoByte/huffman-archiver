#pragma once
#include <stdint.h>

typedef struct HuffmanNode {
    uint8_t wordsize;
    uint8_t* word;
    unsigned long long freq;
    struct HuffmanNode* left;
    struct HuffmanNode* right;
} HuffmanNode;

// create a HuffmanNode with the given parameters
// !!! After use, run "HuffmanNode_freetree" if non-null !!!
HuffmanNode* HuffmanNode_create(
    uint8_t wordsize,
    uint8_t* word,
    unsigned long long freq,
    HuffmanNode* left,
    HuffmanNode* right
);

// Recursively frees the tree of nodes
void HuffmanNode_freetree(HuffmanNode* tree);