// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#include <stdio.h>

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include "minheap.h"
#include "buffio.h"
#include "archiver.h"

#define BUFFER_SIZE 4096

static char check_file_exist(char* filepath) {
    struct stat buffer;
    int exist = stat(filepath, &buffer);
    return exist == 0;
}

static long long get_filesize(char* filepath) {
    struct stat buffer;
    int exist = stat(filepath, &buffer);
    if (exist != 0) {
        return -1;
    }
    return (long long)buffer.st_size;
}

static char* get_filename(char* filepath) {
    char* filename = strrchr(filepath, '/');
    if (filename == NULL)
        filename = strrchr(filepath, '\\'); // Для Windows путей
    if (filename == NULL)
        return filepath; // Если разделителей нет, вернуть весь путь

    return filename + 1; // Пропустить сам разделитель
}

static void logwait(char* msg) {
    printf("%s\n", msg);
    getchar();
}

static void printbin(unsigned char num, unsigned int size) {
    for (int i = sizeof(num)-size; i < sizeof(num); i++) {
        unsigned char bit = num<<i;
        bit >>= sizeof(num)-1;
        printf("%d", bit);
    }
}

// Заполняет список кодов по дереву
// tree - дерево
// codes - список кодов (заполняется)
// curcode - текущий код
// codesize - размер текущего кода в битах
static void Codes_build_reqursion(HuffmanNode* tree, Code* codes, unsigned char* curcode, int codesize) {
    if (tree->left == NULL && tree->right == NULL) {
        unsigned int codesize_bytes = codesize / 8 + (codesize % 8 > 0);
        codes[tree->byte].size = codesize;
        codes[tree->byte].code = (unsigned char*)malloc(codesize_bytes);
        for (int i = 0; i < codesize_bytes; i++) {
            codes[tree->byte].code[i] = curcode[i];
        }
    } else {
        Codes_build_reqursion(tree->left, codes, curcode, codesize + 1);
        unsigned int lastbyte = codesize / 8;
        unsigned char lastbit = codesize % 8;
        curcode[lastbyte] += 1 << (7 - lastbit);
        Codes_build_reqursion(tree->right, codes, curcode, codesize + 1);
        curcode[lastbyte] -= 1 << (7 - lastbit);
    }
}

// Запускает build_codes_reqursion
// tree - дерево
// wordsize - длина слова байтах
// ! Возвращаемое значение требует очистки !
static Codes Codes_build(HuffmanNode* tree, size_t wordsize) {
    Codes codes;
    codes.size = 1 << (wordsize*8);
    codes.codes = (Code*)calloc(codes.size, sizeof(Code));
    unsigned char* curcode = (unsigned char*)calloc(2<<(wordsize*8 - 3), 1);

    Codes_build_reqursion(tree, codes.codes, curcode, 0);
    free(curcode);

    return codes;
}

void Codes_free(Codes codes) {
    free(codes.codes);
}

// Запись дерева в префиксной форме
// tree - дерево
// stream_write - поток вывода
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
// stream_read - поток ввода
// ! Возвращаемое значение требует очистки !
static HuffmanNode* fread_tree(FileBufferIO* stream_read) {
    HuffmanNode* node = HuffmanNode_create(0, 0, NULL, NULL);

    unsigned char state = 0;
    int s = stream_read->readbits(stream_read, &state, 7, 1);
    if (state == 0) {
        node->left = fread_tree(stream_read);
        node->right = fread_tree(stream_read);
    } else {
        stream_read->readbits(stream_read, &node->byte, 0, sizeof(node->byte)*8);
    }

    return node;
}

static void printcodes(Codes codes) {
    for (int i = 0; i < codes.size; i++) {
        int codesize = codes.codes[i].size;
        codesize = codesize / 8 + (codesize % 8 > 0);
        if (codesize==0) {
            continue;
        }
        printf("code ");
        for (int j = 0; j < codesize; j++) {
            if ( codesize - j >= 8 ) {
                printbin(codes.codes[i].code[j], 8);
            } else {
                printbin(codes.codes[i].code[j], codes.codes[i].size % 8);
            }
        }
        printf(" size %d value %c\n", codes.codes[i].size, i);
    }
}

void archive_write_filesize(FileBufferIO* file_archive, FileInfo file_info, unsigned long long filesize, unsigned int treesize) {
    long original_pos = ftell(file_archive->fp);
    fseek(file_archive->fp, file_info.size_pos, SEEK_SET);
    fwrite(&filesize, sizeof(filesize), 1, file_archive->fp);
    fwrite(&treesize, sizeof(treesize), 1, file_archive->fp);
    fseek(file_archive->fp, original_pos, SEEK_SET);
}

FilesInfo prepare_archive(FileBufferIO* file_archive, int files_count, char** filepaths) {
    FilesInfo files_info;
    files_info.total_size = 0;
    files_info.count = 0;
    files_info.files_info = (FileInfo*)malloc(sizeof(FileInfo)*(files_count));

    file_archive->writebits(file_archive, &files_count, 0, sizeof(files_count)*8);

    unsigned long long filesize_default = 0;
    unsigned int treesize_default = 0;
    
    for (int i = 0; i < files_count; i++) {
        FILE* fp = fopen(filepaths[i], "rb");

        char* filename = get_filename(filepaths[i]);
        long long filesize = get_filesize(filepaths[i]);
        if (filesize < 512) {
            printf("WARNING \"%s\" (%lld bytes)\n", filename, filesize);
            printf("File too small, it may be bigger after compressing\n");
            printf("Skip this file? (y/n) ");
            if (getchar() == 'y') {
                filepaths[i][0] =  '\0';
                printf("\n");
                continue;
            };
            printf("\n");
        }

        files_info.total_size += filesize;
        files_info.count++;

        int filename_len = (strlen(filename)+1);
        files_info.files_info[i].name = (char*)malloc(filename_len);
        strcpy(files_info.files_info[i].name, filename);

        file_archive->writebits(file_archive, &filename_len, 0, sizeof(filename_len)*8);
        file_archive->writebits(file_archive, filename, 0, filename_len*8);

        files_info.files_info[i].size_pos = file_archive->byte_p+1;
        file_archive->writebits(file_archive, &filesize_default, 0, sizeof(filesize_default)*8);
        file_archive->writebits(file_archive, &treesize_default, 0, sizeof(treesize_default)*8);
        fclose(fp);
    }
    writebuffer(file_archive);

    // Обновление количества фалов
    long original_pos = ftell(file_archive->fp);
    fseek(file_archive->fp, 0, SEEK_SET);
    fwrite(&files_info.count, sizeof(files_info.count), 1, file_archive->fp);
    fseek(file_archive->fp, original_pos, SEEK_SET);

    return files_info;
}

FilesInfo get_files_info(FileBufferIO* file_archive) {
    FilesInfo files_info;
    files_info.count = 0;
    file_archive->readbits(file_archive, &files_info.count, 0, sizeof(files_info.count)*8);
    files_info.files_info = (FileInfo*)malloc(sizeof(FileInfo)*files_info.count);
    for (int i = 0; i < files_info.count; i++) {
        int filename_len = 0;
        file_archive->readbits(file_archive, &filename_len, 0, sizeof(filename_len)*8);
        files_info.files_info[i].name = (char*)malloc(filename_len);

        file_archive->readbits(file_archive, files_info.files_info[i].name, 0, filename_len*8);
        file_archive->readbits(file_archive, &files_info.files_info[i].size, 0, sizeof(files_info.files_info[i].size)*8);
        file_archive->readbits(file_archive, &files_info.files_info[i].treesize, 0, sizeof(files_info.files_info[i].treesize)*8);
    }

    return files_info;
}

void free_files_info(FilesInfo files_info) {
    for (int i = 0; i < files_info.count; i++) {
        free(files_info.files_info[i].name);
    }
    free(files_info.files_info);
}

void compress(int files_count, char** filepaths, char* archivepath) {
    printf("Compressing starting\n\n");
    FileBufferIO* file_archive = FileBufferIO_open(archivepath, "wb", BUFFER_SIZE);
    FilesInfo files_info = prepare_archive(file_archive, files_count, filepaths);

    int file_num = 0;
    for (int i = 0; i < files_count; i++) {
        char* filename = files_info.files_info[i].name;
        if (filename == NULL) {
            continue;
        }
        long long filesize = get_filesize(filepaths[i]);

        FileBufferIO* file_compress = FileBufferIO_open(filename, "rb", BUFFER_SIZE);
    
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
        Codes codes = Codes_build(tree, 1);
    
        // Сжатие файла
        rewind(file_compress->fp);

        unsigned long long compress_filesize = 0; // Размер сжатого файла в байтах
        unsigned int compress_treesize = fwrite_tree(tree, file_archive); // Размер дерева

        compress_filesize += compress_treesize;

        HuffmanNode_freetree(tree);
    
        // Сжатие со спрессовыванием кодов
        while (file_compress->readbits(file_compress, &byte, 0, sizeof(byte)*8)) {
            compress_filesize += file_archive->writebits(file_archive, codes.codes[byte].code, 0, codes.codes[byte].size);
        }
        Codes_free(codes);
        
        FileBufferIO_close(file_compress); // Файл сжат

        archive_write_filesize(file_archive, files_info.files_info[i], compress_filesize, compress_treesize);
    
        printf("\"%s\" compress result:\n", filename);
        printf("File size: %lld bytes\n", filesize);
        printf("Compressed file size %lld bytes\n\n", compress_filesize/8);    
    }

    FileBufferIO_close(file_archive);

    printf("Files compressed: %d\n", files_info.count);
    printf("Size before compress: %ld bytes\n", files_info.total_size);
    printf("Size after compress: %lld bytes\n", get_filesize(archivepath));

    printf("Saved in %s\n", archivepath); 
}

void decompress(char* dir, char* archivename) {
    FileBufferIO* file_archive = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);

    FilesInfo files_info = get_files_info(file_archive);

    for (int i = 0; i < files_info.count; i++) {
        //FileBufferIO* file_decompress = FileBufferIO_open(files_info.files_info[i].filename, "wb", BUFFER_SIZE);
        char* filename = (char*)malloc(strlen(dir) + strlen(files_info.files_info[i].name) + 2 + i);
        sprintf(filename, "%s/%d%s", dir, i, files_info.files_info[i].name);
        printf("%s\n", filename);
        FileBufferIO* file_decompress = FileBufferIO_open(filename, "wb", BUFFER_SIZE);
        unsigned long long compress_size = files_info.files_info[i].size; // Размер сжатого файла в байтах
        unsigned int compress_treesize = files_info.files_info[i].treesize;  // Размер дерева в битах

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