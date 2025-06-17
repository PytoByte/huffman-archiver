#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffio.h"

// Reading the buffer from the file
void nextbuffer(FileBufferIO* self) {
    self->buffer_readspace = fread(self->buffer, 1, self->buffer_size, self->fp);
    if (self->buffer_readspace == 0) {
        self->byte_p = -1;
        self->bit_p = -1;
    } else {
        self->byte_p = 0;
        self->bit_p = 0;
    }
}

// Writing the buffer to a file
size_t writebuffer(FileBufferIO* self) {
    if (self->byte_p==0 && self->bit_p==0) return 0;

    size_t write_bytes = self->byte_p+(self->bit_p>0);
    if (write_bytes > self->buffer_size) {
        fprintf(stderr, "WARNING! bytes to write out of buffer size\n");
        write_bytes = self->buffer_size;
    }

    size_t wrote_bytes_count = fwrite(self->buffer, 1, write_bytes, self->fp);
    self->byte_p = 0;
    self->bit_p = 0;

    memset(self->buffer, 0, self->buffer_size);
    return wrote_bytes_count;
}

static size_t readbits(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    size_t readed_bits_count_total = 0;

    char* src = (char*)ptr;
    unsigned long long src_pointer = startbit;

    for (size_t i = 0; i < count/8+(count%8 > 0); i++) {
        src[i] = 0;
    }

    while (count > readed_bits_count_total) {
        unsigned char src_bit = src_pointer % 8;
        unsigned long src_byte = src_pointer / 8;

        // Read a new buffer if the data has ended
        if (self->byte_p+(self->bit_p/8) >= self->buffer_readspace) nextbuffer(self);
        if (self->buffer_readspace == 0) break;

        unsigned char reading_bits = self->buffer[self->byte_p] << self->bit_p;
        unsigned char reading_bits_count = 8 - self->bit_p;

        if (reading_bits_count > (count - readed_bits_count_total)) {
            reading_bits_count = (count - readed_bits_count_total);
            reading_bits >>= 8 - (count - readed_bits_count_total);
            reading_bits <<= 8 - (count - readed_bits_count_total);
        }

        // Shift reading pointers
        self->byte_p += (self->bit_p + reading_bits_count) / 8;
        self->bit_p = (self->bit_p + reading_bits_count) % 8;
        src_pointer += reading_bits_count;
        readed_bits_count_total += reading_bits_count; // Increase the number of bits read

        src[src_byte] |= reading_bits >> src_bit; // Write bits to the last byte
        
        unsigned char available_to_write = 8 - src_bit; // How many bits were written
        if (available_to_write < reading_bits_count) {
            unsigned char remain_bits_count = (reading_bits_count - available_to_write); // How many bits were not written
            src_byte++; // Write to the next byte
            src[src_byte] = reading_bits << (reading_bits_count - remain_bits_count);
        }
    }

    return readed_bits_count_total;
}

static size_t writebits(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    size_t wrote_bits_count_total = 0;

    const char* src = (const char*)ptr;
    unsigned long long src_pointer = startbit;

    // Before starting the loop, check whether the buffer is overflowed
    if (self->byte_p+(self->bit_p/8) >= self->buffer_size) writebuffer(self);

    while (count > wrote_bits_count_total) {
        unsigned char src_bit = src_pointer % 8;
        unsigned long long src_byte = src_pointer / 8;

        // How many bits can I write
        unsigned char writing_bits = src[src_byte] << src_bit;
        unsigned char writing_bits_count = 8 - src_bit;

        // Clear to the required amount
        if (writing_bits_count > (count - wrote_bits_count_total)) {
            writing_bits_count = (count - wrote_bits_count_total);
            writing_bits >>= 8 - (count - wrote_bits_count_total);
            writing_bits <<= 8 - (count - wrote_bits_count_total);
        }

        src_pointer += writing_bits_count; // Shift the write pointer
        wrote_bits_count_total += writing_bits_count; // Increase the number of bits written

        self->buffer[self->byte_p] |= writing_bits >> self->bit_p; // Write bits to the last byte of the buffer
        
        unsigned char available_to_write = 8 - self->bit_p; // How many bits were written to the last byte
        if (available_to_write < writing_bits_count) {
            unsigned char remain_bits_count = (writing_bits_count - available_to_write); // How many bits have not been written
            self->byte_p++; // Write to the next byte
            self->bit_p = 0;
            if (self->byte_p+(self->bit_p/8) >= self->buffer_size) writebuffer(self); // But first check that the buffer is not full
            self->bit_p = remain_bits_count;
            self->buffer[self->byte_p] = writing_bits << (writing_bits_count - remain_bits_count);
        } else {
            self->bit_p += writing_bits_count;
        }
    }

    return wrote_bits_count_total;
}

static size_t writebytes(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    return writebits(self, ptr, startbit, count*8);
}

static size_t readbytes(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    return readbits(self, ptr, startbit, count*8);
}

FileBufferIO* FileBufferIO_open(const char* filepath, const char* modes, size_t buffer_size) {
    FileBufferIO* fb = (FileBufferIO*)malloc(sizeof(FileBufferIO));
    fb->fp = fopen(filepath, modes);
    if (fb->fp == NULL) {
        fprintf(stderr, "Can't open file %s\n", filepath);
        free(fb);
        return NULL;
    }
    fb->path = (char*)malloc(strlen(filepath)+1);
    if (fb->path == NULL) {
        fprintf(stderr, "Out of memory\n");
        fclose(fb->fp);
        free(fb);
        return NULL;
    }
    strcpy(fb->path, filepath);

    fb->modes = (char*)malloc(strlen(modes)+1);
    if (fb->modes == NULL) {
        fprintf(stderr, "Out of memory\n");
        fclose(fb->fp);
        free(fb->path);
        free(fb);
        return NULL;
    }
    strcpy(fb->modes, modes);
    fb->buffer = (char*)calloc(buffer_size, sizeof(char));
    if (fb->buffer == NULL) {
        fprintf(stderr, "Out of memory\n");
        fclose(fb->fp);
        free(fb->modes);
        free(fb->path);
        free(fb);
        return NULL;
    }
    fb->buffer_size = buffer_size;
    fb->buffer_readspace = 0;

    if (strchr(fb->modes, 'w')) {
        fb->byte_p = 0;
        fb->bit_p = 0;
    } else {
        fb->byte_p = -1;
        fb->bit_p = -1;
    }
    fb->readbits = readbits;
    fb->writebits = writebits;
    fb->readbytes = readbytes;
    fb->writebytes = writebytes;
    return fb;
}

void FileBufferIO_close(FileBufferIO* fb) {
    if (strchr(fb->modes, 'w') && (fb->bit_p > 0 || fb->byte_p>0)) {
        writebuffer(fb);
    }

    fclose(fb->fp);
    free(fb->path);
    free(fb->modes);
    free(fb->buffer);
    free(fb);
}

void FileBufferIO_close_remove(FileBufferIO* fb) {
    fclose(fb->fp);

    if (remove(fb->path) == 0) {
        printf("Deleted %s.\n", fb->path);
    } else {
        printf("Failed to delete %s.\n", fb->path);
    }

    free(fb->path);
    free(fb->modes);
    free(fb->buffer);
    free(fb);
}
