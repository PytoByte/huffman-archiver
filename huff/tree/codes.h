#pragma once

#include <stdio.h>
#include <stdint.h>

#include "../node.h"

extern uint8_t wordsize;

typedef struct {
    uint8_t* code;
    uint8_t size;
} Code;

typedef struct {
    Code* codes;
    size_t size;
} Codes;

// Converts a word to its index in the codes array
int wordtoi(const uint8_t* word);

// Converts an index of the codes array to a word
void itoword(const int index, uint8_t* word);

void Codes_free(Codes codes);

// Runs build_codes_reqursion
// !!! Don't forget to free the returned string !!!
Codes Codes_build(HuffmanNode* tree);