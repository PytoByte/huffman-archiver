#pragma once

#include <stdio.h>
#include <stdint.h>

#include "../node.h"

extern uint8_t wordsize;

typedef struct TreeBuilder {
    unsigned int size;
    unsigned int capacity;
    HuffmanNode** nodes;
    void (*insert)(struct TreeBuilder* self, HuffmanNode* node); 
    HuffmanNode* (*extract_tree)(struct TreeBuilder* self);      // Extract all nodes in binary tree format
} TreeBuilder;

// Create a TreeBuilder with the given capacity
// !!! After use, run "TreeBuilder_free" if non-null !!!
TreeBuilder* TreeBuilder_create(int capacity);

// Free the TreeBuilder
void TreeBuilder_free(TreeBuilder* tb);