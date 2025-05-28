// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#include <stdio.h>

#include <stdlib.h>
#include <limits.h>

#include "minheap.h"
#include "buffio.h"

#define BUFFER_SIZE 4096

typedef struct {
    unsigned long long code;
    int size;
} Code;

void compress(int files_count, char** filenames, char* archivename);

void decompress(char* dir, char* archivename);