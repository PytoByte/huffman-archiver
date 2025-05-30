#pragma once

typedef struct FileBufferIO {
    FILE* fp;
    char* modes;
    char* buffer;
    size_t buffer_size;
    size_t buffer_readspace;
    unsigned long byte_p;
    unsigned char bit_p;

    size_t (*readbits)(struct FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count);
    size_t (*writebits)(struct FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count);
    size_t (*readbytes)(struct FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count);
    size_t (*writebytes)(struct FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count);
} FileBufferIO;

void nextbuffer(FileBufferIO* self);

size_t writebuffer(FileBufferIO* self);

FileBufferIO* FileBufferIO_open(const char* filename, const char* modes, size_t buffer_size);

void FileBufferIO_close(FileBufferIO* fb);
