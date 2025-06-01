#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffio.h"

// Чтение буфера из файла
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

// Запись буфера в файл
size_t writebuffer(FileBufferIO* self) {
    if (self->byte_p==0 && self->bit_p==0) return 0;

    int write_bytes = self->byte_p+(self->bit_p>0);
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

    for (int i = 0; i < count/8+(count%8 > 0); i++) {
        src[i] = 0;
    }

    while (count > readed_bits_count_total) {
        unsigned char src_bit = src_pointer % 8;
        unsigned long src_byte = src_pointer / 8;

        // Читаю новый буфер, если данные закончились
        if (self->byte_p+(self->bit_p/8) >= self->buffer_readspace) nextbuffer(self);
        if (self->buffer_readspace == 0) break;

        unsigned char reading_bits = self->buffer[self->byte_p] << self->bit_p;
        unsigned char reading_bits_count = 8 - self->bit_p;

        if (reading_bits_count > (count - readed_bits_count_total)) {
            reading_bits_count = (count - readed_bits_count_total);
            reading_bits >>= 8 - (count - readed_bits_count_total);
            reading_bits <<= 8 - (count - readed_bits_count_total);
        }

        // Сдвигаю указатели чтения
        self->byte_p += (self->bit_p + reading_bits_count) / 8;
        self->bit_p = (self->bit_p + reading_bits_count) % 8;
        src_pointer += reading_bits_count;
        readed_bits_count_total += reading_bits_count; // Увеличиваю количество прочитанных бит

        src[src_byte] |= reading_bits >> src_bit; // Пишу биты в последний байт
        
        unsigned char avaiable_to_write = 8 - src_bit; // Сколько бит записалось
        if (avaiable_to_write < reading_bits_count) {
            unsigned char remain_bits_count = (reading_bits_count - avaiable_to_write); // Сколько бит не записалось
            src_byte++; // Запишу в следующий байт
            src[src_byte] = reading_bits << (reading_bits_count - remain_bits_count);
        }
    }

    return readed_bits_count_total;
}

static size_t writebits(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    size_t wrote_bits_count_total = 0;

    const char* src = (const char*)ptr;
    unsigned long long src_pointer = startbit;

    // Перед началом цикла надо проверить не переполнен ли буфер
    if (self->byte_p+(self->bit_p/8) >= self->buffer_size) writebuffer(self);

    while (count > wrote_bits_count_total) {
        unsigned char src_bit = src_pointer % 8;
        unsigned long long src_byte = src_pointer / 8;

        // Сколько бит я могу записать
        unsigned char writing_bits = src[src_byte] << src_bit;
        unsigned char writing_bits_count = 8 - src_bit;

        // Очищаю до нужного количества
        if (writing_bits_count > (count - wrote_bits_count_total)) {
            writing_bits_count = (count - wrote_bits_count_total);
            writing_bits >>= 8 - (count - wrote_bits_count_total);
            writing_bits <<= 8 - (count - wrote_bits_count_total);
        }

        src_pointer += writing_bits_count; // Сдвигаю указатель записи
        wrote_bits_count_total += writing_bits_count; // Увеличиваю количество записаных бит

        self->buffer[self->byte_p] |= writing_bits >> self->bit_p; // Запись битов в последний байт буфера
        
        unsigned char avaiable_to_write = 8 - self->bit_p; // Сколько бит записалось в послдений байт
        if (avaiable_to_write < writing_bits_count) {
            unsigned char remain_bits_count = (writing_bits_count - avaiable_to_write); // Сколько бит не записалось
            self->byte_p++; // Запишу в следующий байт
            self->bit_p = 0;
            if (self->byte_p+(self->bit_p/8) >= self->buffer_size) writebuffer(self); // Но сначала проверю что буфер не заполнен
            self->bit_p = remain_bits_count;
            self->buffer[self->byte_p] = writing_bits << (writing_bits_count - remain_bits_count);
        } else {
            self->bit_p += writing_bits_count;
        }
    }

    return wrote_bits_count_total;
}

static size_t writebytes(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    writebits(self, ptr, startbit, count*8);
}

static size_t readbytes(FileBufferIO* self, const void* ptr, unsigned long long startbit, size_t count) {
    readbits(self, ptr, startbit, count*8);
}

FileBufferIO* FileBufferIO_open(const char* filename, const char* modes, size_t buffer_size) {
    FileBufferIO* fb = (FileBufferIO*)malloc(sizeof(FileBufferIO));
    fb->fp = fopen(filename, modes);
    if (fb->fp == NULL) {
        fprintf(stderr, "Can't open file %s\n", filename);
        free(fb);
        return NULL;
    }
    fb->modes = (char*)malloc(strlen(modes)+1);
    if (fb->modes == NULL) {
        fprintf(stderr, "Out of memory\n");
        free(fb);
        return NULL;
    }
    strcpy(fb->modes, modes);
    fb->buffer = (char*)calloc(buffer_size, sizeof(char));
    if (fb->buffer == NULL) {
        fprintf(stderr, "Out of memory\n");
        free(fb);
        free(fb->modes);
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
    free(fb->modes);
    free(fb->buffer);
    free(fb);
}
