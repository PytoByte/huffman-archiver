#include "node.h"

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

void HuffmanNode_freetree(HuffmanNode* tree) {
    if (tree->left) HuffmanNode_freetree(tree->left);
    if (tree->right) HuffmanNode_freetree(tree->right);
    if (tree->word) free(tree->word);
    free(tree);
}