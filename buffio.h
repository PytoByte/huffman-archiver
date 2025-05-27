#pragma once

typedef struct FileBufferIO {
    FILE* fp;
    char* buffer;
    size_t buffer_size;
    unsigned long byte_p;
    unsigned char bit_p;

    void (*readbits)(struct FileBufferIO* self, void* ptr, size_t count);
    void (*writebits)(struct FileBufferIO* self, void* ptr, size_t count);
} FileBufferIO;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffio.h"

void FileBufferIO_nextbuffer(FileBufferIO* self);

void FileBufferIO_writebuffer(FileBufferIO* self);

void FileBufferIO_readbits(FileBufferIO* self, void* ptr, size_t count);

void FileBufferIO_writebits(FileBufferIO* self, void* ptr, size_t count);

FileBufferIO* FileBufferIO_create(const char* filename, const char* modes, size_t buffer_size);

void FileBufferIO_close(FileBufferIO* fb);
