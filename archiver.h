#pragma once

#define BUFFER_SIZE 4096

#include <stdint.h>

#include "buffio.h"
#include "linkedlist.h"

typedef struct {
    int count;
    int current;
    char* name;
    uint64_t size;
    uint32_t treesize;
    uint64_t filestart;
    FileBufferIO* fb;
} FileFrame;

typedef struct {
    uint64_t original;
    uint64_t compressed;
} FileSize;

typedef struct {
    uint8_t* code;
    uint8_t size;
} Code;

typedef struct {
    Code* codes;
    size_t size;
} Codes;

typedef struct {
    hlist* files;
    unsigned long long total_size;
    int count;
} CompressingFiles;

int compress(char** paths, int paths_count, char* archivepath, uint8_t wordsize_arg);

int decompress(char** paths, int paths_count, char* outdir);