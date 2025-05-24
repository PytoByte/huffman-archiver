#include <stdio.h>

#include "minheap.h"

void build_codes(unsigned char* codes, HuffmanNode* tree, unsigned char cur_code) {
    if (tree->left == NULL && tree->right == NULL) {
        codes[tree->byte] = cur_code;
        return;
    }
    
    if (tree->left != NULL) {
        build_codes(codes, tree->left, cur_code<<1);
    }

    if (tree->right != NULL) {
        build_codes(codes, tree->right, (cur_code<<1) + 1);
    }
}

int main() {
    FILE* target = fopen("example1MB.txt", "rb");

    unsigned long long freqs[UCHAR_MAX+1] = {0};

    unsigned char byte = 0;
    while ((byte = fgetc(target)) != EOF) {
        freqs[byte]++;
    }

    fclose(target);

    MinHeap* heap = MinHeap_create();

    for (unsigned char i = 0; i <= UCHAR_MAX; i++) {
        HuffmanNode* node = HuffmanNode_create(i, freqs[i], NULL, NULL);
        MinHeap_insert(heap, node);
    }

    HuffmanNode* tree = MinHeap_extract_tree(heap);
    free(heap);

    unsigned char codes[UCHAR_MAX+1] = {0};
    build_codes(codes, tree, 0);

    FILE* from = fopen("example1MB.txt", "rb");
    FILE* to = fopen("example1MB.huff", "wb");

    while ((byte = fgetc(from)) != EOF) {
        fwrite(&codes[byte], sizeof(codes[byte]), 1, to);
    }

    fclose(from);
    fclose(to);

    return 0;
}