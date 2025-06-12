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

int wordtoi(const uint8_t* word);

void Codes_free(Codes codes);

// Заполняет список кодов по дереву
// tree - дерево
// codes - список кодов (заполняется)
// curcode - текущий код
// codesize - размер текущего кода в битах
char Codes_build_reqursion(HuffmanNode* tree, Code* codes, uint8_t* curcode, unsigned char codesize);

// Runs build_codes_reqursion
// !!! Don't forget to free the returned string !!!
Codes Codes_build(HuffmanNode* tree);