#include "codes.h"

#include <stdlib.h>
#include <string.h>

int wordtoi(const uint8_t* word) {
    int word_ind = 0;
    memcpy(&word_ind, word, wordsize);
    return word_ind;
}

void Codes_free(Codes codes) {
    for (size_t i = 0; i < codes.size; i++) {
        if (codes.codes[i].size != 0) {
            free(codes.codes[i].code);
            codes.codes[i].size = 0;
        }
    }
    free(codes.codes);
}

void itoword(const int index, uint8_t* word) {
    memcpy(word, &index, wordsize);
}

// Fills codes from Huffman tree
// codes - list where codes will be stored
// set curcode = 0 and codesize = 0 to start recursion
// Returns 0 on success, else 1
static char Codes_build_reqursion(HuffmanNode* tree, Code* codes, uint8_t* curcode, unsigned char codesize) {
    if ((tree->left == NULL && tree->right != NULL) || (tree->left != NULL && tree->right == NULL)) {
        fprintf(stderr, "Corrupted huffman tree\n");
        return 1;
    }

    unsigned int codesize_bytes = codesize / 8 + (codesize % 8 > 0);

    if (tree->left == NULL && tree->right == NULL && codesize == 0 && tree->wordsize < wordsize*8) {
        codesize_bytes = tree->wordsize / 8 + (tree->wordsize % 8 > 0);

        codes[(1 << (wordsize*8))].size = 1;
        codes[(1 << (wordsize*8))].code = (uint8_t*)malloc(codesize_bytes);
        if (!codes[(1 << (wordsize*8))].code) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
        codes[(1 << (wordsize*8))].code[0] = 0;
        return 0;
    } else if (tree->left == NULL && tree->right == NULL && codesize == 0) {
        codesize_bytes = tree->wordsize / 8 + (tree->wordsize % 8 > 0);

        codes[wordtoi(tree->word)].size = 1;
        codes[wordtoi(tree->word)].code = (uint8_t*)malloc(codesize_bytes);
        if (!codes[wordtoi(tree->word)].code) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
        codes[wordtoi(tree->word)].code[0] = 0;
        return 0;
    } else if (tree->left == NULL && tree->right == NULL) {
        int word_index;
        if (tree->wordsize < wordsize*8) {
            word_index = (1 << (wordsize*8));
        } else {
            word_index = wordtoi(tree->word);
        }

        if (codes[word_index].size) {
            printf("\nWARNING: Code for word (ind %d) is already set\n", word_index);
        }

        codes[word_index].size = codesize;
        codes[word_index].code = (uint8_t*)malloc(codesize_bytes);

        if (!codes[word_index].code) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
        for (unsigned int i = 0; i < codesize_bytes; i++) {
            codes[word_index].code[i] = curcode[i];
        }
    } else {
        if (Codes_build_reqursion(tree->left, codes, curcode, codesize + 1) == -1) {
            return 1;
        }
        unsigned int lastbyte = codesize / 8;
        unsigned char lastbit = codesize % 8;
        curcode[lastbyte] += 1 << (7 - lastbit);
        if (Codes_build_reqursion(tree->right, codes, curcode, codesize + 1) == -1) {
            return 1;
        }
        curcode[lastbyte] -= 1 << (7 - lastbit);
    }
    return 0;
}


Codes Codes_build(HuffmanNode* tree) {
    Codes codes;
    codes.size = (1 << (wordsize*8)) + 1;
    codes.codes = (Code*)calloc(codes.size, sizeof(Code));
    if (!codes.codes) {
        codes.size = 0;
        fprintf(stderr, "Out of memory\n");
        return codes;
    }
    uint8_t* curcode = (uint8_t*)calloc(2<<(wordsize*8 - 3), 1);
    if (!curcode) {
        codes.size = 0;
        free(codes.codes);
        fprintf(stderr, "Out of memory\n");
        return codes;
    }

    if (Codes_build_reqursion(tree, codes.codes, curcode, 0) != 0) {
        fprintf(stderr, "Error while building codes\n");
        free(codes.codes);
        free(curcode);
        codes.size = 0;
        return codes;
    }

    free(curcode);
    return codes;
}