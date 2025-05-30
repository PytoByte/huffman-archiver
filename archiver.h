// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#include <stdio.h>

#include <stdlib.h>
#include <limits.h>

#include "minheap.h"
#include "buffio.h"

#define BUFFER_SIZE 4096

typedef struct {
    char* name;

    unsigned long size_pos; // Нужно для перезаписи после бронировнаия места
    unsigned long long size;
    unsigned int treesize;
} FileInfo;

typedef struct {
    int count;
    size_t total_size;
    FileInfo* files_info;
} FilesInfo;

typedef struct {
    unsigned char* code;
    int size;
} Code;

typedef struct {
    Code* codes;
    int size;
} Codes;

void compress(int files_count, char** filenames, char* archivename);

void decompress(char* dir, char* archivename);