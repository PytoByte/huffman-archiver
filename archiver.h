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
    int size;
} Code;

typedef struct {
    Code* codes;
    int size;
} Codes;

typedef struct {
    hlist* files;
    int files_c;
    unsigned long long size;
} CompressingFiles;

int compress(int files_count, char** filenames, char* archivename);

int decompress(char* dir, char* archivename);