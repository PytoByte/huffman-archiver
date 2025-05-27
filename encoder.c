// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#include <stdio.h>

#include <stdlib.h>
#include <limits.h>

#include "minheap.h"

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
void fwrite_tree(HuffmanNode* tree, FILE* stream_write) {
    if (tree->left == NULL && tree->right == NULL) {
        unsigned char state = 1;
        fwrite(&state, sizeof(state), 1, stream_write);
        fwrite(&tree->byte, sizeof(tree->byte), 1, stream_write);
    } else {
        unsigned char state = 0;
        fwrite(&state, sizeof(state), 1, stream_write);
        fwrite_tree(tree->left, stream_write);
        fwrite_tree(tree->right, stream_write);
    }
}

// Чтение дерева из файла
HuffmanNode* fread_tree(FILE* stream_read) {
    HuffmanNode* node = HuffmanNode_create(0, 0, NULL, NULL);

    unsigned char state;
    fread(&state, sizeof(state), 1, stream_read);
    if (state == 0) {
        node->left = fread_tree(stream_read);
        node->right = fread_tree(stream_read);
    } else {
        fread(&node->byte, sizeof(node->byte), 1, stream_read);
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

void encode() {
    FILE* file_encode = fopen("test.txt", "rb");
    long long filesize = get_filesize(file_encode);

    printf("file size: %lld\n", filesize);

    // Подсчёт частоты символов
    unsigned long long freqs[256] = {0};

    unsigned char byte = 0;
    while (fread(&byte, sizeof(byte), 1, file_encode)) {
        freqs[byte]++;
    }

    // Построение дерева
    MinHeap* heap = MinHeap_create();
    for (int i = 0; i < 256; i++) {
        if (freqs[i]==0) {
            continue;
        }
        HuffmanNode* node = HuffmanNode_create((unsigned char)i, freqs[i], NULL, NULL);
        MinHeap_insert(heap, node);
    }

    HuffmanTree tree_withinfo = MinHeap_extract_tree(heap);
    free(heap);

    // Кодировка по дереву
    Code codes[256] = {0};
    build_codes(codes, tree_withinfo.tree, 0, 0);
    //printcodes(codes, 256);

    // Сжатия файла
    rewind(file_encode);

    // Сначала во временный файл без дополнительных данных
    FILE* file_temp = fopen("temp", "wb");

    unsigned long long filesize_total = 0; // Размер сжатого файла в байтах
    unsigned char filesize_tail = 0;       // Используемые биты в последнем байте (хвосте)

    // Сжатие со спрессовыванием кодов
    unsigned char byte_fill = 0;
    unsigned char byte_writing = 0;
    while (fread(&byte, sizeof(byte), 1, file_encode)) {
        if (byte_fill+codes[byte].size < 8) {
            byte_fill += codes[byte].size;
            byte_writing += codes[byte].code << (8 - byte_fill);
        } else {
            byte_writing += (codes[byte].code << (8 - codes[byte].size)) >> byte_fill;
            fwrite(&byte_writing, sizeof(byte_writing), 1, file_temp);
            filesize_total++;
            unsigned char remover = codes[byte].code << ((8 - codes[byte].size) + (8 - byte_fill));

            byte_fill = codes[byte].size - (8 - byte_fill);
            byte_writing = remover;
        }
    }

    // Запись хвоста
    if (byte_fill>0) {
        filesize_tail = byte_fill;
        fwrite(&byte_writing, sizeof(byte_writing), 1, file_temp);
        filesize_total++;
    }

    fclose(file_encode); // Файл сжат
    fclose(file_temp); // file_temp содержит сжатый file_encode, без дополнительный данных

    // Запись file_temp в file_archive с дополнительными данными
    file_temp = fopen("temp", "rb");

    // Данные для расшифровки
    FILE* file_archive = fopen("archive.huff", "wb");
    printf("compressed size %llu\n", filesize_total);
    fwrite(&filesize_total, sizeof(filesize_total), 1, file_archive);
    fwrite(&filesize_tail, sizeof(filesize_tail), 1, file_archive);
    fwrite_tree(tree_withinfo.tree, file_archive);

    while (fread(&byte, sizeof(byte), 1, file_temp)) {
        fwrite(&byte, sizeof(byte), 1, file_archive);
    }
    
    fclose(file_temp);

    fclose(file_archive);
}

void decode() {
    FILE* file_archive = fopen("archive.huff", "rb");

    unsigned long long filesize_total = 0; // Размер сжатого файла в байтах
    unsigned char filesize_tail = 0;       // Используемые биты в последнем байте (хвосте)

    fread(&filesize_total, sizeof(filesize_total), 1, file_archive);
    fread(&filesize_tail, sizeof(filesize_tail), 1, file_archive);
    HuffmanNode* tree = fread_tree(file_archive);

    Code codes[256];
    build_codes(codes, tree, 0, 0);
    //printcodes(codes, 256);

    HuffmanNode* node_cur = tree;
    for (unsigned long long i = 0; i < filesize_total; i++) {
        unsigned char byte;
        fread(&byte, sizeof(byte), 1, file_archive);

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
                printf("%c", node_cur->byte);
                node_cur = tree;

                if (i == filesize_total-1 && j == filesize_tail) {
                    break;
                }
            }
        }
    }
    printf("\n");

    fclose(file_archive);
}

int main() {
    encode();
    decode();

    return 0;
}