#include <stdio.h>
#include <stdlib.h>

#include "buffio.h"

void FileBufferIO_nextbuffer(FileBufferIO* self) {
    self->byte_p = 0;
    self->bit_p = 0;
    fread(self->buffer, self->buffer_size, 1, self->fp);
}

void FileBufferIO_writebuffer(FileBufferIO* self) {
    self->byte_p = 0;
    self->bit_p = 0;
    fwrite(self->buffer, self->buffer_size, 1, self->fp);
}

void FileBufferIO_readbits(FileBufferIO* self, void* ptr, size_t count) {
    char* ptr_bytes = (char*)ptr;
    unsigned long ptr_byte = 0;
    unsigned char ptr_bit = 0;

    while (count) {
        // next buffer if current end
        if (self->byte_p >= self->buffer_size) FileBufferIO_nextbuffer(self);

        unsigned char readed_bits = 0;
        unsigned char readed_bits_count = 0;
        if ((size_t)self->bit_p + count > 7) {
            // clear bits from before readed
            readed_bits = self->buffer[self->byte_p] << self->bit_p;
            readed_bits >> self->bit_p;
    
            // get readed bits count
            readed_bits_count = 8 - self->bit_p;
            // change count of requiring bits
            count -= readed_bits_count;
    
            // shift pointers
            self->byte_p++;
            self->bit_p = 0;
        } else {
            // clear bits from readed
            readed_bits = self->buffer[self->byte_p] << self->bit_p;
            // get exact required bits
            readed_bits >>= (8 - count);
    
            // get readed bits count
            readed_bits_count = count;
            // change count of requiring bits
            count = 0;
    
            // shift pointers
            self->bit_p += readed_bits_count;
        }
    
        if (ptr_bit + readed_bits_count > 7) {
            // write readed bits
            ptr_bytes[ptr_byte] += readed_bits >> (readed_bits_count - (8 - ptr_bit));

            if (ptr_bit - (8 - readed_bits_count) == 0) {
                ptr_byte++;
                continue;
            }
    
            // get remain bits at right position
            unsigned char bits_remain = readed_bits << (8 - (readed_bits_count - (8 - ptr_bit)));
            // write remain bits in next byte
            ptr_byte++;
            ptr_bytes[ptr_byte] += bits_remain;
            // shift bit pointer
            ptr_bit = (readed_bits_count - (8 - ptr_bit));
        } else {
            // write readed bits
            readed_bits <<= (8 - readed_bits_count);
            ptr_bytes[ptr_byte] += readed_bits >> ptr_bit;
            // shift bit pointer
            ptr_bit += readed_bits_count;
        }
    }
}

void FileBufferIO_writebits(FileBufferIO* self, void* ptr, size_t count) {
    char* ptr_bytes = (char*)ptr;
    unsigned long ptr_byte = 0;
    unsigned char ptr_bit = 0;

    while (count) {
        if (self->byte_p >= self->buffer_size) FileBufferIO_writebuffer(self);

        unsigned char writing_bits = 0;
        unsigned char writing_bits_count = 0;
        
        if (count > 8) {
            writing_bits = ptr_bytes[ptr_byte];
            writing_bits_count = 8;
            count -= writing_bits_count;
            ptr_byte++;
        } else {
            writing_bits = ptr_bytes[ptr_byte] >> (8 - count);
            writing_bits_count = count;
            count = 0;
        }
    
        if (self->bit_p + writing_bits_count > 7) {
            // write bits
            self->buffer[self->byte_p] += writing_bits >> (self->bit_p - (8 - writing_bits_count));
            if (self->bit_p - (8 - writing_bits_count) == 0) {
                self->byte_p++;
                continue;
            }
    
            // get remain bits at right position
            unsigned char bits_remain = writing_bits << (8 - (self->bit_p - (8 - writing_bits_count)));
            // write remain bits in next byte
            self->byte_p++;
            if (self->byte_p >= self->buffer_size) FileBufferIO_writebuffer(self);
            self->buffer[self->byte_p] += bits_remain;
            // shift bit pointer
            self->bit_p = (self->bit_p - (8 - writing_bits_count));
        } else {
            // write readed bits
            writing_bits <<= (8 - writing_bits_count);
            self->buffer[self->byte_p] += writing_bits >> self->bit_p;
            // shift bit pointer
            self->bit_p += writing_bits_count;
        }
    }
}

FileBufferIO* FileBufferIO_create(const char* filename, const char* modes, size_t buffer_size) {
    FileBufferIO* fb = (FileBufferIO*)malloc(sizeof(FileBufferIO));
    fb->fp = fopen(filename, modes);
    fb->buffer = (char*)malloc(buffer_size);
    fb->buffer_size = buffer_size;
    fb->byte_p = -1;
    fb->bit_p = -1;
    fb->readbits = FileBufferIO_readbits;
    fb->writebits = FileBufferIO_writebits;
    return fb;
}

void FileBufferIO_close(FileBufferIO* fb) {
    fclose(fb->fp);
    free(fb);
}
