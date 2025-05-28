// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#include <stdio.h>

#include <stdlib.h>
#include <limits.h>

#include "minheap.h"
#include "buffio.h"

#define BUFFER_SIZE 4096

typedef struct {
    unsigned char code;
    int size;
} Code;

void build_codes(Code* codes, HuffmanNode* tree, unsigned char cur_code, int code_size) {
    if (tree->left == NULL && tree->right == NULL) {
        codes[tree->byte] = (Code){.code=cur_code, .size=code_size};
        return;
    }
    
    if (tree->left != NULL) {
        build_codes(codes, tree->left, cur_code<<1, code_size+1);
    }

    if (tree->right != NULL) {
        build_codes(codes, tree->right, (cur_code<<1) + 1, code_size+1);
    }
}

// Запись дерева в префиксной форме
size_t fwrite_tree(HuffmanNode* tree, FileBufferIO* stream_write) {
    size_t tree_size = 0;

    if (tree->left == NULL && tree->right == NULL) {
        unsigned char state = 1;
        tree_size += stream_write->writebits(stream_write, &state, 7, 1);
        tree_size += stream_write->writebits(stream_write, &tree->byte, 0, sizeof(tree->byte)*8);
    } else {
        unsigned char state = 0;
        tree_size += stream_write->writebits(stream_write, &state, 7, 1);
        tree_size += fwrite_tree(tree->left, stream_write);
        tree_size += fwrite_tree(tree->right, stream_write);
    }
    return tree_size;
}

// Чтение дерева из файла
HuffmanNode* fread_tree(FileBufferIO* stream_read) {
    HuffmanNode* node = HuffmanNode_create(0, 0, NULL, NULL);

    unsigned char state;
    stream_read->readbits(stream_read, &state, 7, 1);
    if (state == 0) {
        node->left = fread_tree(stream_read);
        node->right = fread_tree(stream_read);
    } else {
        stream_read->readbits(stream_read, &node->byte, 0, sizeof(node->byte)*8);
    }

    return node;
}

void printbin(unsigned int code, unsigned int size) {
    for (int i = 8-size; i < 8; i++) {
        unsigned char bit = code<<i;
        bit >>= 7;
        printf("%d", bit);
    }
}

void printcodes(Code* codes, unsigned int size) {
    for (int i = 0; i < size; i++) {
        if (codes[i].size==0) {
            continue;
        }
        printf("code ");
        printbin(codes[i].code, codes[i].size);
        printf(" size %d value %c\n", codes[i].size, i);
    }
}

long long get_filesize(FILE* fp) {
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return (long long)size;
}

void compress(char* filename, char* archivename) {
    FileBufferIO* file_compress = FileBufferIO_open(filename, "rb", BUFFER_SIZE);
    long long filesize = get_filesize(file_compress->fp);

    // Подсчёт частоты символов
    unsigned long long freqs[256] = {0};

    unsigned char byte = 0;

    while (file_compress->readbits(file_compress, &byte, 0, sizeof(byte)*8)) {
        freqs[byte]++;
    }

    // Построение дерева
    MinHeap* heap = MinHeap_create();
    for (int i = 0; i < 256; i++) {
        if (freqs[i]==0) {
            continue;
        }
        HuffmanNode* node = HuffmanNode_create((unsigned char)i, freqs[i], NULL, NULL);
        heap->insert(heap, node);
    }

    HuffmanNode* tree = heap->extract_tree(heap);
    MinHeap_free(heap);

    // Кодировка по дереву
    Code codes[256] = {0};
    build_codes(codes, tree, 0, 0);
    //printcodes(codes, 256);

    // Сжатия файла
    rewind(file_compress->fp);

    // Сначала во временный файл без дополнительных данных
    FileBufferIO* file_temp = FileBufferIO_open("temp", "wb", BUFFER_SIZE);

    unsigned long long compress_size_total = 0;
    unsigned long long compress_size_file = 0; // Размер сжатого файла в байтах
    unsigned char compress_size_tail = 0;       // Используемые биты в последнем байте (хвосте)

    // Сжатие со спрессовыванием кодов
    while (file_compress->readbits(file_compress, &byte, 0, sizeof(byte)*8)) {
        compress_size_file += file_temp->writebits(file_temp, &codes[byte].code, 8-codes[byte].size, codes[byte].size);
    }
    compress_size_tail = compress_size_file % 8;
    compress_size_file = compress_size_file / 8 + (compress_size_tail > 0);

    FileBufferIO_close(file_compress); // Файл сжат
    FileBufferIO_close(file_temp); // file_temp содержит сжатый file_compress, без дополнительный данных

    // Запись file_temp в file_archive с дополнительными данными
    file_temp = FileBufferIO_open("temp", "rb", BUFFER_SIZE);

    // Данные для расшифровки
    FileBufferIO* file_archive = FileBufferIO_open(archivename, "wb", BUFFER_SIZE);

    compress_size_total += file_archive->writebits(file_archive, &compress_size_file, 0, sizeof(compress_size_file)*8);
    compress_size_total += file_archive->writebits(file_archive, &compress_size_tail, 0, sizeof(compress_size_tail)*8);
    compress_size_total += fwrite_tree(tree, file_archive);
    HuffmanNode_freetree(tree);

    while (file_temp->readbits(file_temp, &byte, 0, sizeof(byte)*8)) {
        file_archive->writebits(file_archive, &byte, 0, sizeof(byte)*8);
    }
    
    FileBufferIO_close(file_temp);
    FileBufferIO_close(file_archive);

    compress_size_total = (compress_size_total / 8) + (compress_size_total % 8 > 0) + compress_size_file;

    printf("Files compress result:\n");
    printf("File size: %lld bytes\n", filesize);
    printf("Compressed size: %lld bytes\n", compress_size_file);
    printf("Archive size total: %lld bytes\n", compress_size_total);
    printf("Saved in %s\n", archivename);

    return;
}

void decompress(char* archivename) {
    FileBufferIO* file_archive = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);

    unsigned long long compress_size_total = 0; // Размер сжатого файла в байтах
    unsigned char compress_size_tail = 0;       // Используемые биты в последнем байте (хвосте)

    file_archive->readbits(file_archive, &compress_size_total, 0, sizeof(compress_size_total)*8);
    file_archive->readbits(file_archive, &compress_size_tail, 0, sizeof(compress_size_tail)*8);
    HuffmanNode* tree = fread_tree(file_archive);

    Code codes[256];
    build_codes(codes, tree, 0, 0);
    //printcodes(codes, 256);

    HuffmanNode* node_cur = tree;
    for (unsigned long long i = 0; i < compress_size_total; i++) {
        unsigned char byte;
        file_archive->readbits(file_archive, &byte, 0, sizeof(byte)*8);

        for (int j = 0; j < 8; j++) {
            unsigned char bit = byte<<j;
            bit >>= 7;
            //printf(" (%d %d %d %d) ", byte, bit, 7+j, 7);
            if (bit==0) {
                node_cur = node_cur->left;
            } else if (bit==1) {
                node_cur = node_cur->right;
            }
            //printf("%d) (bit: %d) (cur byte: %c)\n", j, bit, node_cur->byte);
            
            if (node_cur->left==NULL && node_cur->right==NULL) {
                //printf("%c", node_cur->byte);
                node_cur = tree;

                if (i == compress_size_total-1 && j == compress_size_tail) {
                    break;
                }
            }
        }
    }
    printf("\n");

    FileBufferIO_close(file_archive);
}

int main() {
    compress("test.txt", "archive.huff");
    decompress("archive.huff");

    return 0;
}