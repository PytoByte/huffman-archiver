#pragma once

typedef struct FileBufferIO {
    FILE* fp;
    char* modes;
    char* buffer;
    size_t buffer_size;
    size_t buffer_readspace;
    unsigned long byte_p;
    unsigned char bit_p;

    size_t (*readbits)(struct FileBufferIO* self, void* ptr, size_t count);
    void (*writebits)(struct FileBufferIO* self, void* ptr, size_t count);
} FileBufferIO;

#include <stdio.h>
#include <stdlib.h>

FileBufferIO* FileBufferIO_create(const char* filename, const char* modes, size_t buffer_size);

void FileBufferIO_close(FileBufferIO* fb);
