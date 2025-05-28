// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#include <stdio.h>

#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "minheap.h"
#include "buffio.h"
#include "archiver.h"

#define BUFFER_SIZE 4096

static void printbin(unsigned long long code, unsigned int size) {
    for (int i = 8-size; i < 8; i++) {
        unsigned char bit = code<<i;
        bit >>= 7;
        printf("%d", bit);
    }
}

static void build_codes(Code* codes, HuffmanNode* tree, unsigned long long cur_code, int code_size) {
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
static size_t fwrite_tree(HuffmanNode* tree, FileBufferIO* stream_write) {
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
static HuffmanNode* fread_tree(FileBufferIO* stream_read) {
    HuffmanNode* node = HuffmanNode_create(0, 0, NULL, NULL);

    unsigned char state = 0;
    stream_read->readbits(stream_read, &state, 7, 1);
    if (state == 0) {
        node->left = fread_tree(stream_read);
        node->right = fread_tree(stream_read);
    } else {
        stream_read->readbits(stream_read, &node->byte, 0, sizeof(node->byte)*8);
    }

    return node;
}

static void printcodes(Code* codes, unsigned int size) {
    for (int i = 0; i < size; i++) {
        if (codes[i].size==0) {
            continue;
        }
        printf("code ");
        printbin(codes[i].code, codes[i].size);
        printf(" size %d value %c\n", codes[i].size, i);
    }
}

static long long get_filesize(FILE* fp) {
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return (long long)size;
}

typedef struct {
    char* filename;
    unsigned long name_pos;
    unsigned long long filesize;
    unsigned int treesize;
    unsigned long size_pos;
} FileInfo;

typedef struct {
    int files_count;
    size_t total_size;
    unsigned long files_start;
    FileInfo* files_info;
} FilesInfo;

void archive_write_filesize(FileBufferIO* file_archive, FileInfo file_info, unsigned long long filesize, unsigned int treesize) {
    long original_pos = ftell(file_archive->fp);
    fseek(file_archive->fp, file_info.size_pos, SEEK_SET);
    fwrite(&filesize, sizeof(filesize), 1, file_archive->fp);
    fwrite(&treesize, sizeof(treesize), 1, file_archive->fp);
    fseek(file_archive->fp, original_pos, SEEK_SET);
}

FilesInfo prepare_archive(FileBufferIO* file_archive, int* files_count, char** filenames) {
    FilesInfo files_info;
    files_info.total_size = 0;
    files_info.files_info = (FileInfo*)malloc(sizeof(FileInfo)*(*files_count));

    file_archive->writebits(file_archive, files_count, 0, sizeof(*files_count)*8);

    unsigned long long filesize_default = 0;
    unsigned int treesize_default = 0;
    
    for (int i = 0; i < *files_count; i++) {
        char* filename = filenames[i];
        FILE* fp = fopen(filename, "rb");
        unsigned long long filesize = get_filesize(fp);
        /*if (filesize < 512) {
            printf("WARNING \"%s\" (%lld bytes)\n", filename, filesize);
            printf("File too small, it may be bigger after compressing\n");
            printf("Skip this file? (y/n)\n");
            if (getchar() == 'y') {
                filenames[i][0] =  '\0';
                *files_count--;
                printf("\n");
                continue;
            };
            printf("\n");
        }*/

        files_info.total_size += filesize;

        files_info.files_info[i].name_pos = file_archive->byte_p;
        file_archive->writebits(file_archive, filename, 0, (strlen(filename)+1)*8);

        files_info.files_info[i].size_pos = file_archive->byte_p;
        file_archive->writebits(file_archive, &filesize_default, 0, sizeof(filesize_default)*8);
        file_archive->writebits(file_archive, &treesize_default, 0, sizeof(treesize_default)*8);
        fclose(fp);
    }
    writebuffer(file_archive);

    long original_pos = ftell(file_archive->fp);
    fseek(file_archive->fp, 0, SEEK_SET);
    fwrite(files_count, sizeof(*files_count), 1, file_archive->fp);
    fseek(file_archive->fp, original_pos, SEEK_SET);

    return files_info;
}

FilesInfo get_files_info(FileBufferIO* file_archive) {
    FilesInfo files_info;
    files_info.files_count = 0;
    file_archive->readbits(file_archive, &files_info.files_count, 0, sizeof(files_info.files_count)*8);
    files_info.files_info = (FileInfo*)malloc(sizeof(FileInfo)*files_info.files_count);
    for (int i = 0; i < files_info.files_count; i++) {
        int namesize = strlen(&file_archive->buffer[file_archive->byte_p]);
        files_info.files_info[i].filename = (char*)malloc(sizeof(char)*namesize+1);

        file_archive->readbits(file_archive, &files_info.files_info[i].filename, 0, (namesize+1)*8);
        file_archive->readbits(file_archive, &files_info.files_info[i].filesize, 0, sizeof(files_info.files_info[i].filesize)*8);
        file_archive->readbits(file_archive, &files_info.files_info[i].treesize, 0, sizeof(files_info.files_info[i].treesize)*8);
        files_info.files_start = file_archive->byte_p;
    }

    return files_info;
}

void compress(int files_count, char** filenames, char* archivename) {
    printf("Compressing starting\n\n");
    FileBufferIO* file_archive = FileBufferIO_open(archivename, "wb", BUFFER_SIZE);
    FilesInfo files_info = prepare_archive(file_archive, &files_count, filenames);
    unsigned long long files_size_total = files_info.total_size;

    printf("Files count: %d\n", files_count);
    printf("Size before compress: %lld\n\n", files_size_total);

    int file_num = 0;
    for (int i = 0; i < files_count; i++) {
        char* filename = filenames[i];
        if (strlen(filename) == 0) {
            continue;
        }

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
        printcodes(codes, 256);
    
        // Сжатие файла
        rewind(file_compress->fp);

        unsigned long long compress_filesize = 0; // Размер сжатого файла в байтах
        unsigned int compress_treesize = 0;      // Используемые биты в последнем байте (хвосте)

        compress_treesize += fwrite_tree(tree, file_archive);
        compress_filesize += compress_treesize;

        printf("\n");
        HuffmanNode_freetree(tree);
    
        // Сжатие со спрессовыванием кодов
        while (file_compress->readbits(file_compress, &byte, 0, sizeof(byte)*8)) {
            printf("%c\n", byte);
            compress_filesize += file_archive->writebits(file_archive, &codes[byte].code, 64-codes[byte].size, codes[byte].size);
        }
        
        FileBufferIO_close(file_compress); // Файл сжат

        archive_write_filesize(file_archive, files_info.files_info[i], compress_filesize, compress_treesize);
    
        printf("\"%s\" compress result:\n", filename);
        printf("File size: %lld bytes\n", filesize);
        printf("Compressed file size %lld bytes\n\n", compress_filesize/8);       
    }
    FileBufferIO_close(file_archive);

    printf("Saved in %s\n", archivename); 
}

void decompress(char* dir, char* archivename) {
    FileBufferIO* file_archive = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);

    FilesInfo files_info = get_files_info(file_archive);

    for (int i = 0; i < files_info.files_count; i++) {
        //FileBufferIO* file_decompress = FileBufferIO_open(files_info.files_info[i].filename, "wb", BUFFER_SIZE);
        FileBufferIO* file_decompress = FileBufferIO_open("demo.txt", "wb", BUFFER_SIZE);
        unsigned long long compress_size = files_info.files_info[i].filesize; // Размер сжатого файла в байтах
        unsigned int compress_treesize = files_info.files_info[i].treesize;  // Используемые биты в последнем байте (хвосте)

        HuffmanNode* tree = fread_tree(file_archive);

        HuffmanNode* node_cur = tree;
        for (unsigned long long i = 0; i < compress_size - compress_treesize; i++) {
            unsigned char bit = 0;
            file_archive->readbits(file_archive, &bit, 0, 1);
            bit >>= 7;

            if (bit==0) {
                node_cur = node_cur->left;
            } else if (bit==1) {
                node_cur = node_cur->right;
            }
            
            if (node_cur->left==NULL && node_cur->right==NULL) {
                //printf("%c", node_cur->byte);
                file_decompress->writebits(file_decompress, &node_cur->byte, 0, sizeof(node_cur->byte)*8);
                node_cur = tree;
            }
        }

        HuffmanNode_freetree(tree);
        FileBufferIO_close(file_decompress);
    }

    FileBufferIO_close(file_archive);
}