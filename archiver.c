#include <stdio.h>

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "minheap.h"
#include "buffio.h"
#include "archiver.h"
#include "progbar.h"

static uint8_t wordsize = 1;

void printbinary(unsigned char* num, int bits) {
    for (int i = 0; i < bits; i++) {
        printf("%d", (num[0] >> (bits - i - 1)) & 1);
    }
}

// Возвращает количество цифр в числе
static int digits_count(int num) {
    int c = 0;
    while (num > 0) {
        num /= 10;
        c++;
    }
    return c;
}

// == Работа с файлами ===========================
static int check_file_exist(const char* filepath) {
    struct stat buffer;
    return stat(filepath, &buffer) == 0;
}

static size_t get_filesize(const char* filepath) {
    struct stat buffer;
    int exist = stat(filepath, &buffer);
    if (exist == -1) {
        perror("stat error");
        return 0;
    }
    return buffer.st_size;
}

static char* get_filename(char* filepath) {
    char* filename = strrchr(filepath, '/');
    if (filename == NULL)
        return filepath; // Если разделителей нет, вернуть весь путь

    return filename + 1; // Пропустить сам разделитель
}

// ! Возвращаемое значение очистить !
static char* generate_unique_filepath(const char* path_old) {
    // Если файл не существует, возвращаем оригинальный путь
    char* path = (char*)malloc(strlen(path_old)+1);
    strcpy(path, path_old);

    if (!check_file_exist(path)) {
        return path;
    }

    // Разбираем имя файла и расширение
    char* dot_pos = strrchr(get_filename(path), '.');

    char* ext = NULL;
    if (dot_pos) {
        ext = (char*)malloc(strlen(dot_pos)+1);
        if (!ext) {
            fprintf(stderr, "Out of memory\n");
            free(path);
            return NULL;
        }
        strcpy(ext, dot_pos);
        dot_pos[0] = 0;
    }
    
    char* name = (char*)malloc(strlen(path)+1);
    if (!name) {
        fprintf(stderr, "Out of memory\n");
        free(path);
        if (ext) free(ext);
        return NULL;
    }
    strcpy(name, path);

    // Генерируем новые имена с добавлением "(n)"
    unsigned int addition_num = 1;
    do {
        int ext_len = 0;
        if (ext) ext_len = strlen(ext);
        path = realloc(path, strlen(name) + ext_len + digits_count(addition_num) + 6);
        if (!path) {
            fprintf(stderr, "Out of memory\n");
            free(name);
            if (ext) free(ext);
            free(path);
            return NULL;
        };

        if (ext) {
            sprintf(path, "%s (%u)%s", name, addition_num, ext);
        } else {
            sprintf(path, "%s (%u)", name, addition_num);
        }
        addition_num++;
    } while (check_file_exist(path));

    free(name);
    if (ext) free(ext);

    return path;
}
// == Работа с файлами ===========================


// == Построение кодов по дереву Хаффмана ========
static int wordtoi(const uint8_t* word) {
    int word_ind = 0;
    memcpy(&word_ind, word, wordsize);
    return word_ind;
}

static void Codes_free(Codes codes) {
    for (int i = 0; i < codes.size; i++) {
        if (codes.codes[i].size != 0) {
            //printf("Freeing code %d\n", i); // DEBUG
            free(codes.codes[i].code);
            codes.codes[i].size = 0;
        }
    }
    free(codes.codes);
}

// Заполняет список кодов по дереву
// tree - дерево
// codes - список кодов (заполняется)
// curcode - текущий код
// codesize - размер текущего кода в битах
static char Codes_build_reqursion(HuffmanNode* tree, Code* codes, uint8_t* curcode, int codesize) {
    if ((tree->left == NULL && tree->right != NULL) || (tree->left != NULL && tree->right == NULL)) {
        fprintf(stderr, "Corrupted huffman tree\n");
        return -1;
    }

    unsigned int codesize_bytes = codesize / 8 + (codesize % 8 > 0);

    if (tree->left == NULL && tree->right == NULL && codesize == 0 && tree->wordsize < wordsize*8) {
        codesize_bytes = tree->wordsize / 8 + (tree->wordsize % 8 > 0);

        codes[(1 << (wordsize*8))].size = 1;
        codes[(1 << (wordsize*8))].code = (uint8_t*)malloc(codesize_bytes);
        if (!codes[(1 << (wordsize*8))].code) {
            fprintf(stderr, "Out of memory\n");
            return -1;
        }
        //printf("Setting code last %d\n", 1 << (wordsize*8)); // DEBUG
        codes[(1 << (wordsize*8))].code[0] = 0;
        return 0;
    } else if (tree->left == NULL && tree->right == NULL && codesize == 0) {
        codesize_bytes = tree->wordsize / 8 + (tree->wordsize % 8 > 0);

        codes[wordtoi(tree->word)].size = 1;
        codes[wordtoi(tree->word)].code = (uint8_t*)malloc(codesize_bytes);
        if (!codes[wordtoi(tree->word)].code) {
            fprintf(stderr, "Out of memory\n");
            return -1;
        }
        //printf("Setting code single %d\n", wordtoi(tree->word, tree->wordsize)); // DEBUG
        codes[wordtoi(tree->word)].code[0] = 0;
        return 0;
    } else if (tree->left == NULL && tree->right == NULL) {
        int word_index;
        if (tree->wordsize < wordsize*8) {
            word_index = (1 << (wordsize*8));
        } else {
            word_index = wordtoi(tree->word);
        }

        if (codes[word_index].size) {
            printf("word "); // DEBUG
            printbinary(tree->word, tree->wordsize); // DEBUG
            printf("\n"); // DEBUG
            printf("WARNING: Code for word (ind %d): (code ", word_index); // DEBUG
            printbinary(codes[word_index].code, codes[word_index].size);
            printf(") is already set\n"); // DEBUG
        }

        codes[word_index].size = codesize;
        codes[word_index].code = (uint8_t*)malloc(codesize_bytes);

        //printf("Setting code leaf %ld %d\n", codes[word_index].size, word_index); // DEBUG
        if (!codes[word_index].code) {
            fprintf(stderr, "Out of memory\n");
            return -1;
        }
        for (int i = 0; i < codesize_bytes; i++) {
            codes[word_index].code[i] = curcode[i];
        }
    } else {
        if (Codes_build_reqursion(tree->left, codes, curcode, codesize + 1) == -1) {
            return -1;
        }
        unsigned int lastbyte = codesize / 8;
        unsigned char lastbit = codesize % 8;
        curcode[lastbyte] += 1 << (7 - lastbit);
        if (Codes_build_reqursion(tree->right, codes, curcode, codesize + 1) == -1) {
            return -1;
        }
        curcode[lastbyte] -= 1 << (7 - lastbit);
    }
    return 0;
}

// Запускает build_codes_reqursion
// tree - дерево
// wordsize - длина слова байтах
// ! Возвращаемое значение требует очистки !
static Codes Codes_build(HuffmanNode* tree) {
    Codes codes;
    codes.size = (1 << (wordsize*8)) + 1;
    codes.codes = (Code*)calloc(codes.size, sizeof(Code));
    if (!codes.codes) {
        codes.size = 0;
        fprintf(stderr, "Out of memory\n");
        return codes;
    }
    uint8_t* curcode = (uint8_t*)calloc(2<<(wordsize*8 - 3), 1);
    if (!curcode) {
        codes.size = 0;
        free(codes.codes);
        fprintf(stderr, "Out of memory\n");
        return codes;
    }

    if (Codes_build_reqursion(tree, codes.codes, curcode, 0) != 0) {
        fprintf(stderr, "Error while building codes\n");
        free(codes.codes);
        free(curcode);
        codes.size = 0;
        return codes;
    }

    free(curcode);
    return codes;
}
// == Построение кодов по дереву Хаффмана ========

// == Чтение и запись дерева (префиксная форма) ==
// Запись дерева в префиксной форме
// tree - дерево
// stream_write - поток вывода
static size_t fwrite_tree(HuffmanNode* tree, FileBufferIO* stream_write) {
    size_t tree_size = 0;
    if (tree->left == NULL && tree->right == NULL) {
        unsigned char state = 1;
        tree_size += stream_write->writebits(stream_write, &state, 7, 1);
        unsigned char lastword = (tree->wordsize != wordsize*8);
        tree_size += stream_write->writebits(stream_write, &lastword, 7, 1);
        if (lastword) {
            tree_size += stream_write->writebytes(stream_write, &tree->wordsize, 0, sizeof(tree->wordsize));
        }
        tree_size += stream_write->writebits(stream_write, tree->word, 0, tree->wordsize);
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
static HuffmanNode* fread_tree(FileBufferIO* stream_read, uint32_t treesize, unsigned int readed) {
    if (readed > treesize) {
        fprintf(stderr, "Corrupted huffman tree - out of bounds\n");
        return NULL;
    }

    HuffmanNode* node = HuffmanNode_create(0, NULL, 0, NULL, NULL);
    if (!node) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }

    uint8_t state = 0;
    if (stream_read->readbits(stream_read, &state, 7, 1) != 1) {
        fprintf(stderr, "Corrupted huffman tree - EOF\n");
        HuffmanNode_freetree(node);
        return NULL;
    }

    if (state == 0) {
        node->left = fread_tree(stream_read, treesize, readed+1);
        if (!node->left) {
            HuffmanNode_freetree(node);
            return NULL;
        }
        
        node->right = fread_tree(stream_read, treesize, readed+1);
        if (!node->right) {
            HuffmanNode_freetree(node);
            return NULL;
        }
    } else {
        uint8_t lastword = 0;
        if (stream_read->readbits(stream_read, &lastword, 7, 1) != 1) {
            fprintf(stderr, "Corrupted huffman tree - EOF\n");
            HuffmanNode_freetree(node);
            return NULL;
        }

        node->wordsize = wordsize*8;
        if (lastword) {
            if (!stream_read->readbytes(stream_read, &node->wordsize, 0, sizeof(node->wordsize))) {
                fprintf(stderr, "Corrupted huffman tree - EOF\n");
                HuffmanNode_freetree(node);
                return NULL;
            }
        }

        node->word = (uint8_t*)calloc(wordsize, 1);
        if (!node->word) {
            fprintf(stderr, "Out of memory\n");
            HuffmanNode_freetree(node);
            return NULL;
        }

        if (!stream_read->readbits(stream_read, node->word, 0, node->wordsize)) {
            fprintf(stderr, "Corrupted huffman tree - EOF\n");
            HuffmanNode_freetree(node);
            return NULL;
        }
    }

    return node;
}
// == Чтение и запись дерева (префиксная форма) ==



// == Подготовка архива ==========================
static long prepare_file(FileBufferIO* archive, char* path) {
   if (strncmp(path, "./", 2) == 0) {
       path += 2;
   }

   uint32_t filename_len = strlen(path)+1;
   uint64_t compressed_filesize = 0;
   uint32_t compressed_treesize = 0;
   uint64_t filestart = 0;

   archive->writebytes(archive, &filename_len, 0, sizeof(filename_len));
   archive->writebytes(archive, path, 0, filename_len);

   long size_pos = ftell(archive->fp);
   if (size_pos == -1) return -1;
   size_pos = (size_pos + archive->byte_p) + 1;

   archive->writebytes(archive, &compressed_filesize, 0, sizeof(compressed_filesize));
   archive->writebytes(archive, &compressed_treesize, 0, sizeof(compressed_treesize));
   archive->writebytes(archive, &filestart, 0, sizeof(filestart));

   return size_pos;
}


typedef struct {
    int added;
    unsigned long long filesize;
} PreparedFiles;

static PreparedFiles prepare_allfiles(FileBufferIO* archive, hlist* list, char* startpath, char* addpath) {
    char* path = (char*)malloc(strlen(startpath) + strlen(addpath) + 1);
    strcpy(path, startpath);
    strcat(path, addpath);

    struct stat file_stat;
    if (stat(path, &file_stat) == -1) {
        perror("stat error");
        free(path);
        return (PreparedFiles){-1, 0};
    }

    if (S_ISREG(file_stat.st_mode)) {
        size_t filesize = get_filesize(path);
        /*if (filesize < 512) {
            printf("WARNING \"%s\" (%ld bytes)\n", get_filename(path), filesize);
            printf("File too small, it may be bigger after compressing\n");
            printf("Skip this file? (y/n) ");
            if (getchar() == 'y') {
                free(path);
                return 0;
            };
            printf("\n");
        }*/

        if (strlen(addpath) == 0) {
            long size_pos = prepare_file(archive, get_filename(startpath));
            if (size_pos == -1) {
                free(path);
                fprintf(stderr, "Getting file position error\n");
                return (PreparedFiles){-1, 0};
            }
            addtolist(list, path, size_pos);
        } else {
            char* rootdir = get_filename(startpath);
            char* filepath = (char*)malloc(strlen(rootdir) + strlen(addpath) + 1);
            if (!filepath) {
                fprintf(stderr, "Out of memory\n");
                free(path);
                return (PreparedFiles){-1, 0};
            }
            strcpy(filepath, rootdir);
            strcat(filepath, addpath);
            long size_pos = prepare_file(archive, filepath);
            free(filepath);
            if (size_pos == -1) {
                free(path);
                fprintf(stderr, "Getting file position error\n");
                return (PreparedFiles){-1, 0};
            }
            addtolist(list, path, size_pos);
        }
        free(path);
        return (PreparedFiles){1, filesize};
    }

    if (!S_ISDIR(file_stat.st_mode)) {
        free(path);
        return (PreparedFiles){0, 0};
    }

    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL) {
        perror("Open dir error");
        free(path);
        return (PreparedFiles){0, 0};
    }

    PreparedFiles files = {0, 0};
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char* new_addpath = (char*)malloc(strlen(addpath) + strlen(entry->d_name) + 2);
        sprintf(new_addpath, "%s/%s", addpath, entry->d_name);

        PreparedFiles temp = prepare_allfiles(archive, list, startpath, new_addpath);
        files.added += temp.added;
        files.filesize += temp.filesize;

        free(new_addpath);
    }

    free(path);
    closedir(dir);

    return files;
}

static void archive_write_filesize(FileBufferIO* archive, long size_pos, uint64_t filesize, uint32_t treesize, uint64_t filestart) {
    long original_pos = ftell(archive->fp);
    fseek(archive->fp, size_pos, SEEK_SET);
    fwrite(&filesize, sizeof(filesize), 1, archive->fp);
    fwrite(&treesize, sizeof(treesize), 1, archive->fp);
    fwrite(&filestart, sizeof(filestart), 1, archive->fp);
    fseek(archive->fp, original_pos, SEEK_SET);
}

// Бронирует место для заголовков в начале архива
// Возвращает список файлов и указателей для сжания и заполнения заголовков
static CompressingFiles prepare_archive(FileBufferIO* archive, int paths_c, char** paths) {
    CompressingFiles compr_files;

    compr_files.files = initlist();
    compr_files.count = 0;
    compr_files.total_size = 0;
    archive->writebytes(archive, &compr_files.count, 0, sizeof(compr_files.count));
    archive->writebytes(archive, &wordsize, 0, sizeof(wordsize));

    for (int i = 0; i < paths_c; i++) {
        PreparedFiles temp = prepare_allfiles(archive, compr_files.files, paths[i], "");
        if (temp.added == -1) {
            freelist(compr_files.files);
            compr_files.files = NULL;
            compr_files.count = 0;
            return compr_files;
        }
        compr_files.count += temp.added;
        compr_files.total_size += temp.filesize;
    }
    writebuffer(archive);

    // Обновление количества файлов
    long original_pos = ftell(archive->fp);
    if (original_pos == -1) {
        fprintf(stderr, "Getting file position error\n");
        freelist(compr_files.files);
        compr_files.files = NULL;
        compr_files.count = 0;
        return compr_files;
    }
    
    if (fseek(archive->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Fseek error\n");
        freelist(compr_files.files);
        compr_files.files = NULL;
        compr_files.count = 0;
        return compr_files;
    }

    fwrite(&compr_files.count, sizeof(compr_files.count), 1, archive->fp);

    if (fseek(archive->fp, original_pos, SEEK_SET) != 0) {
        fprintf(stderr, "Fseek error\n");
        freelist(compr_files.files);
        compr_files.files = NULL;
        compr_files.count = 0;
        return compr_files;
    }

    return compr_files;
}
// == Подготовка архива ==========================


// == Получение файлов из архива =================
static void end_fileframe(FileFrame* fileframe) {
    free(fileframe->name);
    fileframe->current = fileframe->count;
}

static FileFrame get_fileframe(FileBufferIO* archive, int count) {
    FileFrame fileframe;
    fileframe.fb = archive;
    fileframe.count = count;
    fileframe.current = 1;
    fileframe.filestart = 0;
    fileframe.size = 0;
    fileframe.treesize = 0;

    int filename_len = 0;
    if (!archive->readbytes(archive, &filename_len, 0, sizeof(filename_len))) {
        fprintf(stderr, "EOF while reading headers\n");
        fileframe.count = -1;
        return fileframe;
    }

    fileframe.name = (char*)malloc(filename_len);
    if (!fileframe.name) {
        fprintf(stderr, "Out of memory\n");
        fileframe.count = -1;
        return fileframe;
    }

    if (!archive->readbytes(archive, fileframe.name, 0, filename_len)) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(&fileframe);
        fileframe.count = -1;
        return fileframe;
    }
    if (!archive->readbytes(archive, &fileframe.size, 0, sizeof(fileframe.size))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(&fileframe);
        fileframe.count = -1;
        return fileframe;
    }
    if (!archive->readbytes(archive, &fileframe.treesize, 0, sizeof(fileframe.treesize))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(&fileframe);
        fileframe.count = -1;
        return fileframe;
    }
    if (!archive->readbytes(archive, &fileframe.filestart, 0, sizeof(fileframe.filestart))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(&fileframe);
        fileframe.count = -1;
        return fileframe;
    }

    return fileframe;
}

static int next_fileframe(FileFrame* fileframe) {
    FileBufferIO* archive = fileframe->fb;

    free(fileframe->name);
    if (fileframe->current == fileframe->count) {
        return 0;
    }
    fileframe->current++;

    unsigned int filename_len = 0;
    archive->readbytes(archive, &filename_len, 0, sizeof(filename_len));
    fileframe->name = (char*)calloc(filename_len, 1);
    if (fileframe->name == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }

    if (!archive->readbytes(archive, fileframe->name, 0, filename_len)) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(fileframe);
        return 0;
    }
    fileframe->size = 0;
    if (!archive->readbytes(archive, &fileframe->size, 0, sizeof(fileframe->size))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(fileframe);
        return 0;
    }
    fileframe->treesize = 0;
    if (!archive->readbytes(archive, &fileframe->treesize, 0, sizeof(fileframe->treesize))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(fileframe);
        return 0;
    }
    fileframe->filestart = 0;
    if (!archive->readbytes(archive, &fileframe->filestart, 0, sizeof(fileframe->filestart))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_fileframe(fileframe);
        return 0;
    }

    return 1;
}
// == Получение файлов из архива =================

// == Сжатие файлов ==============================
static FileSize compress_file(FileBufferIO* archive, const char* path, long size_pos) {
    FileSize filesize;
    filesize.compressed = 0;
    filesize.original = get_filesize(path);
    if (filesize.original == 0) {
        return filesize;
    }
    
    long filestart = ftell(archive->fp);
    if (filestart < 0) {
        fprintf(stderr, "Getting file position error\n");
        return filesize;
    }
    filestart = filestart * 8 + archive->byte_p * 8 + archive->bit_p;
    FileBufferIO* file_compress = FileBufferIO_open(path, "rb", BUFFER_SIZE);
    if (!file_compress) {
        return filesize;
    }

    // Подсчёт частоты символов
    int freqs_size = (1 << (wordsize*8));
    unsigned long long* freqs = (unsigned long long*)calloc(freqs_size, sizeof(unsigned long long)); // Частоты символов (для кодирования по дереву
    if (!freqs) {
        fprintf(stderr, "Out of memory\n");
        FileBufferIO_close(file_compress);
        return filesize;
    }

    uint8_t* word = (uint8_t*)calloc(wordsize, sizeof(uint8_t));
    if (!word) {
        fprintf(stderr, "Out of memory\n");
        free(freqs);
        FileBufferIO_close(file_compress);
        return filesize;
    }

    uint8_t* lastword = NULL;
    uint8_t lastword_size = 0;

    while (1) {
        size_t readed = file_compress->readbytes(file_compress, word, 0, wordsize);

        if (readed == 0) {
            break;
        }

        if (readed != wordsize*8) {
            lastword = (uint8_t*)calloc(wordsize, sizeof(uint8_t));
            if (!lastword) {
                fprintf(stderr, "Out of memory\n");
                free(freqs);
                free(word);
                FileBufferIO_close(file_compress);
                return filesize;
            }
            strncpy(lastword, word, wordsize);
            lastword_size = readed;
        } else {
            freqs[wordtoi(word)]++;
        }
        pg_update(readed);
    }

    // Построение дерева
    MinHeap* heap = MinHeap_create(freqs_size+1);
    for (unsigned int i = 0; i < freqs_size; i++) {
        if (freqs[i]==0) {
            continue;
        }
        memcpy(word, &i, wordsize);
        //printf("%d\n", word[0]); // DEBUG
        //printf("extracted word from %d: ", i); // DEBUG
        //printbinary(word, wordsize*8); // DEBUG
        //printf("\n"); // DEBUG
        HuffmanNode* node = HuffmanNode_create(wordsize*8, word, freqs[i], NULL, NULL);
        if (!node) {
            fprintf(stderr, "Out of memory\n");
            FileBufferIO_close(file_compress);
            free(freqs);
            free(word);
            free(lastword);
            MinHeap_free(heap);
            return filesize;
        }
        heap->insert(heap, node);
    }

    if (lastword) {
        HuffmanNode* node = HuffmanNode_create(lastword_size, lastword, 1, NULL, NULL);
        if (!node) {
            fprintf(stderr, "Out of memory\n");
            FileBufferIO_close(file_compress);
            free(freqs);
            free(word);
            free(lastword);
            MinHeap_free(heap);
            return filesize;
        }
        heap->insert(heap, node);
        free(lastword);
    }

    free(freqs);

    HuffmanNode* tree = heap->extract_tree(heap);
    MinHeap_free(heap);

    // Кодировка по дереву
    Codes codes = Codes_build(tree);
    if (codes.size == 0) {
        free(word);
        FileBufferIO_close(file_compress);
        return filesize;
    }

    // Сжатие файла
    rewind(file_compress->fp);

    uint32_t compress_treesize = fwrite_tree(tree, archive); // Размер дерева в битах
    filesize.compressed = compress_treesize; // Размер сжатого файла в битах

    HuffmanNode_freetree(tree);

    // Сжатие со спрессовыванием кодов
    while (1) {
        size_t readed = file_compress->readbytes(file_compress, word, 0, wordsize);

        if (readed == 0) {
            break;
        }

        uint8_t* code;
        size_t size;
        if (readed != wordsize*8) {
            code = codes.codes[codes.size-1].code;
            size = codes.codes[codes.size-1].size;
        } else {
            code = codes.codes[wordtoi(word)].code;
            size = codes.codes[wordtoi(word)].size;
        }
        filesize.compressed += archive->writebits(archive, code, 0, size);
        pg_update(readed);
    }
    free(word);
    Codes_free(codes);
    
    FileBufferIO_close(file_compress);

    archive_write_filesize(archive, size_pos, filesize.compressed, compress_treesize, filestart);

    return filesize;
}

int compress(char** paths, int paths_count, char* archivepath, uint8_t wordsize_arg) {
    wordsize = wordsize_arg;

    if (paths_count == 0) {
        fprintf(stderr, "Nothing to compress\n");
        return 1;
    }

    char* unique_archivepath = generate_unique_filepath(archivepath);
    FileBufferIO* archive = FileBufferIO_open(unique_archivepath, "wb", BUFFER_SIZE);
    if (!archive) {
        free(unique_archivepath);
        return 1;
    }
    CompressingFiles compr_files = prepare_archive(archive, paths_count, paths);

    if (compr_files.files == NULL) {
        free(unique_archivepath);
        FileBufferIO_close(archive);
        return 1;
    } else if (compr_files.count == 0) {
        fprintf(stderr, "Nothing to compress\n");
        free(unique_archivepath);
        freelist(compr_files.files);
        FileBufferIO_close(archive);
        return 1;
    }

    FileSize filesize_total = {.original = 0, .compressed = 0};

    pg_init(compr_files.count + compr_files.total_size*16, 0);
    llist* cur = compr_files.files->begin;
    while (cur != NULL) {
        if (strcmp(get_filename(cur->path), unique_archivepath) == 0) {
            cur = cur->next;
            continue;
        }
        FileSize filesize = compress_file(archive, cur->path, cur->size_pos);
        //getchar(); // DEBUG
        if (filesize.compressed == 0 && filesize.original != 0) {
            pg_end();
            free(unique_archivepath);
            fprintf(stderr, "Error while compressing %s\n", cur->path);
            return 1;
        }
        filesize_total.original += filesize.original;
        pg_update(1);
        cur = cur->next;
    }
    freelist(compr_files.files);
    pg_end();

    FileBufferIO_close(archive);

    printf("Result: %ld -> %ld bytes\n", filesize_total.original, get_filesize(unique_archivepath));
    printf("Saved in %s\n", unique_archivepath);
    free(unique_archivepath);
    return 0;
}
// == Сжатие файлов ==============================

// == Распаковка файлов ==========================
static int create_directories(const char *path) {
    char* pp = NULL;
    char* sp = NULL;

    char* temp = (char*)malloc(strlen(path)+1);
    if (!temp) {
        fprintf(stderr, "Out of memory");
        return 1;
    }
    strcpy(temp, path);

    pp = temp;
    while ((sp = strchr(pp, '/')) != NULL) {
        if (sp != temp) { // не корневая директория "/"
            *sp = '\0';
            if (check_file_exist(temp) == 0) {
                if (mkdir(temp, 0700) == -1) {
                    perror("mkdir");
                    free(temp);
                    return 1;
                }
            }
            *sp = '/';
        }
        pp = sp + 1;
    }

    free(temp);
    return 0;
}

int decompress(char** paths, int paths_count, char* outdir) {
    if (paths_count == 0) {
        fprintf(stderr, "Nothing to decompress\n");
        return 1;
    }

    for (int i = 0; i < paths_count; i++) {
        char* archivepath = paths[i];
        FileBufferIO* archive = FileBufferIO_open(archivepath, "rb", BUFFER_SIZE);
        if (!archive) {
            return 1;
        }

        FileBufferIO* archive_frame = FileBufferIO_open(archivepath, "rb", BUFFER_SIZE);
        if (!archive_frame) {
            FileBufferIO_close(archive);
            return 1;
        }

        uint32_t files_count = 0;
        if (!archive_frame->readbytes(archive_frame, &files_count, 0, sizeof(files_count))) {
            fprintf(stderr, "EOF while reading headers\n");
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }

        if (!archive_frame->readbytes(archive_frame, &wordsize, 0, sizeof(wordsize))) {
            fprintf(stderr, "EOF while reading headers\n");
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }

        FileFrame fileframe = get_fileframe(archive_frame, files_count);
        if (fileframe.count == -1) {
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }

        printf("Decompressing files from %s\n", archivepath);
        pg_init(get_filesize(archivepath)*8, 0);
        do {
            int fs = fseek(archive->fp, fileframe.filestart / 8, SEEK_SET);
            if (fs != 0) {
                pg_end();
                fprintf(stderr, "Fseek error\n");
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                return 1;
            }

            nextbuffer(archive);
            archive->bit_p = fileframe.filestart % 8;

            char* path = (char*)malloc(strlen(outdir) + strlen(fileframe.name) + 2);
            if (!path) {
                pg_end();
                fprintf(stderr, "Out of memory\n");
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                return 1;
            }
            sprintf(path, "%s/%s", outdir, fileframe.name);
            if (create_directories(path) != 0) {
                pg_end();
                free(path);
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                return 1;
            }

            char* unique_path = generate_unique_filepath(path);
            free(path);
            if (!unique_path) {
                pg_end();
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                return 1;
            }
            FileBufferIO* file_decompress = FileBufferIO_open(unique_path, "wb", BUFFER_SIZE);
            if (!file_decompress) {
                pg_end();
                free(unique_path);
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                return 1;
            }
            free(unique_path);

            if (fileframe.treesize == 0) {
                FileBufferIO_close(file_decompress);
                continue;
            }

            HuffmanNode* tree = fread_tree(archive, fileframe.treesize, 0);
            if (!tree) {
                pg_end();
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                FileBufferIO_close(file_decompress);
                return 1;
            }

            HuffmanNode* node_cur = tree;
            for (unsigned long long i = 0; i < fileframe.size - fileframe.treesize; i++) {
                unsigned char bit = 0;
                size_t readed = 0;
                if (!(readed = archive->readbits(archive, &bit, 7, 1))) {
                    pg_end();
                    fprintf(stderr, "EOF while decompressing\n");
                    end_fileframe(&fileframe);
                    FileBufferIO_close(archive);
                    FileBufferIO_close(archive_frame);
                    FileBufferIO_close(file_decompress);
                    HuffmanNode_freetree(tree);
                    return 1;
                }

                if (node_cur==tree && node_cur->wordsize!=0) {
                    if (bit==0) {
                        file_decompress->writebits(file_decompress, node_cur->word, 0, node_cur->wordsize);
                        continue;
                    } else {
                        pg_end();
                        fprintf(stderr, "Corrupted huffman tree - unexpected bit\n");
                        end_fileframe(&fileframe);
                        FileBufferIO_close(archive);
                        FileBufferIO_close(archive_frame);
                        FileBufferIO_close(file_decompress);
                        HuffmanNode_freetree(tree);
                        return 1;
                    }
                }

                if (bit==0) {
                    node_cur = node_cur->left;
                } else if (bit==1) {
                    node_cur = node_cur->right;
                }

                if (!node_cur) {
                    pg_end();
                    fprintf(stderr, "Corrupted huffman tree or file\n");
                    end_fileframe(&fileframe);
                    FileBufferIO_close(archive);
                    FileBufferIO_close(archive_frame);
                    FileBufferIO_close(file_decompress);
                    HuffmanNode_freetree(tree);
                    return 1;
                }
                
                if (node_cur->left==NULL && node_cur->right==NULL) {
                    file_decompress->writebits(file_decompress, node_cur->word, 0, node_cur->wordsize);
                    node_cur = tree;
                }
                pg_update(readed);
            }

            HuffmanNode_freetree(tree);
            FileBufferIO_close(file_decompress);
        } while (next_fileframe(&fileframe));

        FileBufferIO_close(archive);
        FileBufferIO_close(archive_frame);

        pg_end();
    }

    return 0;
}
// == Распаковка файлов ==========================