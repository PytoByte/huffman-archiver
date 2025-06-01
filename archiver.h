#pragma once

#define BUFFER_SIZE 4096

#include "linkedlist.h"

typedef struct {
    int count;
    int current;
    char* name;
    unsigned long long size;
    unsigned int treesize;
    unsigned long long filestart;
} FileFrame;

typedef struct {
    unsigned long long original;
    unsigned long long compressed;
} FileSize;

typedef struct {
    unsigned char* code;
    size_t size;
} Code;

typedef struct {
    Code* codes;
    size_t size;
} Codes;

typedef struct {
    hlist* files;
    int files_c;
} CompressingFiles;

int compress(char** paths, int paths_count, char* archivepath, int wordsize_arg);

int decompress(char** paths, int paths_count, char* outdir);