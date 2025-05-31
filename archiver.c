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

// Возвращает количество цифр в числе
int digits_count(int num) {
    int c = 0;
    while (num > 0) {
        num /= 10;
        c++;
    }
    return c;
}

// == Работа с файлами ===========================
static int check_file_exist(char* filepath) {
    struct stat buffer;
    return stat(filepath, &buffer) == 0;
}

static size_t get_filesize(char* filepath) {
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
        filename = strrchr(filepath, '\\'); // Для Windows путей
    if (filename == NULL)
        return filepath; // Если разделителей нет, вернуть весь путь

    return filename + 1; // Пропустить сам разделитель
}

// ! Возвращаемое значение очистить !
char* generate_unique_filepath(const char* path_old) {
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
void Codes_free(Codes codes) {
    for (int i = 0; i < codes.size; i++) {
        if (codes.codes[i].size != 0) {
            free(codes.codes[i].code);
        }
    }
    free(codes.codes);
}

// Заполняет список кодов по дереву
// tree - дерево
// codes - список кодов (заполняется)
// curcode - текущий код
// codesize - размер текущего кода в битах
static char Codes_build_reqursion(HuffmanNode* tree, Code* codes, unsigned char* curcode, int codesize) {
    if ((tree->left == NULL && tree->right != NULL) || (tree->left != NULL && tree->right == NULL)) {
        fprintf(stderr, "Corrupted huffman tree\n");
        return -1;
    }
    if (tree->left == NULL && tree->right == NULL) {
        unsigned int codesize_bytes = codesize / 8 + (codesize % 8 > 0);
        codes[tree->byte].size = codesize;
        codes[tree->byte].code = (unsigned char*)malloc(codesize_bytes);
        if (!codes[tree->byte].code) {
            fprintf(stderr, "Out of memory\n");
            return -1;
        }
        for (int i = 0; i < codesize_bytes; i++) {
            codes[tree->byte].code[i] = curcode[i];
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
}

// Запускает build_codes_reqursion
// tree - дерево
// wordsize - длина слова байтах
// ! Возвращаемое значение требует очистки !
static Codes Codes_build(HuffmanNode* tree, size_t wordsize) {
    Codes codes;
    codes.size = 1 << (wordsize*8);
    codes.codes = (Code*)calloc(codes.size, sizeof(Code));
    if (!codes.codes) {
        codes.size = 0;
        free(codes.codes);
        fprintf(stderr, "Out of memory\n");
        return codes;
    }
    unsigned char* curcode = (unsigned char*)calloc(2<<(wordsize*8 - 3), 1);
    if (!curcode) {
        codes.size = 0;
        free(codes.codes);
        fprintf(stderr, "Out of memory\n");
        return codes;
    }

    Codes_build_reqursion(tree, codes.codes, curcode, 0);
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
static HuffmanNode* fread_tree(FileBufferIO* stream_read, unsigned int treesize, unsigned int readed) {
    if (readed > treesize) {
        fprintf(stderr, "Corrupted huffman tree - out of bounds\n");
        return NULL;
    }

    HuffmanNode* node = HuffmanNode_create(0, 0, NULL, NULL);
    if (!node) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }

    unsigned char state = 0;
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
        if (!stream_read->readbytes(stream_read, &node->byte, 0, sizeof(node->byte))) {
            fprintf(stderr, "Corrupted huffman tree - EOF\n");
            HuffmanNode_freetree(node);
            return NULL;
        }
    }

    return node;
}
// == Чтение и запись дерева (префиксная форма) ==



// == Подготовка архива ==========================
long prepare_file(FileBufferIO* archive, char* path) {
   long size_pos = 0;

   if (strncmp(path, "./", 2) == 0) {
       path += 2;
   }

   unsigned int filename_len = strlen(path)+1;
   unsigned long long compressed_filesize = 0;
   unsigned int compressed_treesize = 0;
   unsigned long long filestart = 0;

   archive->writebytes(archive, &filename_len, 0, sizeof(filename_len));
   archive->writebytes(archive, path, 0, filename_len);
   size_pos = ftell(archive->fp);
   if (size_pos == -1) return -1;
   size_pos = (size_pos + archive->byte_p) + 1;
   archive->writebytes(archive, &compressed_filesize, 0, sizeof(compressed_filesize));
   archive->writebytes(archive, &compressed_treesize, 0, sizeof(compressed_treesize));
   archive->writebytes(archive, &filestart, 0, sizeof(filestart));

   return size_pos;
}


int prepare_allfiles(FileBufferIO* archive, hlist* list, char* startpath, char* addpath) {
    char* path = (char*)malloc(strlen(startpath) + strlen(addpath) + 1);
    strcpy(path, startpath);
    strcat(path, addpath);

    struct stat file_stat;
    if (stat(path, &file_stat) == -1) {
        perror("stat error");
        free(path);
        return -1;
    }

    if (S_ISREG(file_stat.st_mode)) {
        size_t filesize = get_filesize(path);
        if (filesize < 512) {
            printf("WARNING \"%s\" (%ld bytes)\n", get_filename(path), filesize);
            printf("File too small, it may be bigger after compressing\n");
            printf("Skip this file? (y/n) ");
            if (getchar() == 'y') {
                free(path);
                return 0;
            };
            printf("\n");
        }

        if (strlen(addpath) == 0) {
            long size_pos = prepare_file(archive, get_filename(startpath));
            if (size_pos == -1) {
                free(path);
                fprintf(stderr, "Getting file position error\n");
                return -1;
            }
            addtolist(list, path, size_pos);
        } else {
            char* rootdir = get_filename(startpath);
            char* filepath = (char*)malloc(strlen(rootdir) + strlen(addpath) + 1);
            if (!filepath) {
                fprintf(stderr, "Out of memory\n");
                free(path);
                return -1;
            }
            strcpy(filepath, rootdir);
            strcat(filepath, addpath);
            long size_pos = prepare_file(archive, filepath);
            free(filepath);
            if (size_pos == -1) {
                free(path);
                fprintf(stderr, "Getting file position error\n");
                return -1;
            }
            addtolist(list, path, size_pos);
        }
        free(path);
        return 1;
    }

    if (!S_ISDIR(file_stat.st_mode)) {
        free(path);
        return 0;
    }

    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL) {
        perror("Open dir error");
        free(path);
        return 0;
    }

    int files_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char* new_addpath = (char*)malloc(strlen(addpath) + strlen(entry->d_name) + 2);
        sprintf(new_addpath, "%s/%s", addpath, entry->d_name);

        files_count += prepare_allfiles(archive, list, startpath, new_addpath);

        free(new_addpath);
    }

    free(path);
    closedir(dir);

    return files_count;
}

void archive_write_filesize(FileBufferIO* archive, long long size_pos, unsigned long long filesize, unsigned int treesize, unsigned long long filestart) {
    long original_pos = ftell(archive->fp);
    fseek(archive->fp, size_pos, SEEK_SET);
    fwrite(&filesize, sizeof(filesize), 1, archive->fp);
    fwrite(&treesize, sizeof(treesize), 1, archive->fp);
    fwrite(&filestart, sizeof(filestart), 1, archive->fp);
    fseek(archive->fp, original_pos, SEEK_SET);
}

// Бронирует место для заголовков в начале архива
// Возвращает список файлов и указателей для сжания и заполнения заголовков
CompressingFiles prepare_archive(FileBufferIO* archive, int paths_c, char** paths) {
    CompressingFiles compr_files;

    compr_files.files_c = 0;
    archive->writebytes(archive, &compr_files.files_c, 0, sizeof(compr_files.files_c));

    compr_files.files = initlist();

    for (int i = 0; i < paths_c; i++) {
        int temp = prepare_allfiles(archive, compr_files.files, paths[i], "");
        if (temp == -1) {
            freelist(compr_files.files);
            compr_files.files = NULL;
            compr_files.files_c = 0;
            return compr_files;
        }
        compr_files.files_c += temp;
        size_t tempsize = get_filesize(paths[i]);
        if (!tempsize) {
            freelist(compr_files.files);
            compr_files.files = NULL;
            compr_files.files_c = 0;
            return compr_files;
        }
        compr_files.size += tempsize;
    }
    writebuffer(archive);

    // Обновление количества файлов
    long original_pos = ftell(archive->fp);
    if (original_pos == -1) {
        fprintf(stderr, "Getting file position error\n");
        freelist(compr_files.files);
        compr_files.files = NULL;
        compr_files.files_c = 0;
        return compr_files;
    }
    
    if (fseek(archive->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Fseek error\n");
        freelist(compr_files.files);
        compr_files.files = NULL;
        compr_files.files_c = 0;
        return compr_files;
    }

    fwrite(&compr_files.files_c, sizeof(compr_files.files_c), 1, archive->fp);

    if (fseek(archive->fp, original_pos, SEEK_SET) != 0) {
        fprintf(stderr, "Fseek error\n");
        freelist(compr_files.files);
        compr_files.files = NULL;
        compr_files.files_c = 0;
        return compr_files;
    }

    return compr_files;
}
// == Подготовка архива ==========================


// == Получение файлов из архива =================
void end_fileframe(FileFrame* fileframe) {
    free(fileframe->name);
    fileframe->current = fileframe->count;
}

FileFrame get_fileframe(FileBufferIO* archive) {
    FileFrame fileframe;
    fileframe.count = 0;
    fileframe.current = 1;
    fileframe.filestart = 0;
    fileframe.size = 0;
    fileframe.treesize = 0;

    if (!archive->readbytes(archive, &fileframe.count, 0, sizeof(fileframe.count))) {
        fprintf(stderr, "EOF while reading headers\n");
        fileframe.count = -1;
        return fileframe;
    }

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

char next_fileframe(FileFrame* fileframe, FileBufferIO* archive) {
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
FileSize compress_file(FileBufferIO* archive, char* path, long long size_pos) {
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
    unsigned long long freqs[256] = {0};
    unsigned char byte = 0;
    while (file_compress->readbytes(file_compress, &byte, 0, sizeof(byte))) {
        freqs[byte]++;
        pg_update(1);
    }

    // Построение дерева
    MinHeap* heap = MinHeap_create();
    for (int i = 0; i < 256; i++) {
        if (freqs[i]==0) {
            continue;
        }
        HuffmanNode* node = HuffmanNode_create((unsigned char)i, freqs[i], NULL, NULL);
        if (!node) {
            FileBufferIO_close(file_compress);
            MinHeap_free(heap);
            fprintf(stderr, "Out of memory\n");
            return filesize;
        }
        heap->insert(heap, node);
    }
    HuffmanNode* tree = heap->extract_tree(heap);
    MinHeap_free(heap);

    // Кодировка по дереву
    Codes codes = Codes_build(tree, 1);
    if (codes.size == 0) {
        FileBufferIO_close(file_compress);
        return filesize;
    }

    // Сжатие файла
    rewind(file_compress->fp);

    unsigned int compress_treesize = fwrite_tree(tree, archive); // Размер дерева в битах
    filesize.compressed = compress_treesize; // Размер сжатого файла в битах

    HuffmanNode_freetree(tree);

    // Сжатие со спрессовыванием кодов
    while (file_compress->readbytes(file_compress, &byte, 0, sizeof(byte))) {
        filesize.compressed += archive->writebits(archive, codes.codes[byte].code, 0, codes.codes[byte].size);
        pg_update(1);
    }
    Codes_free(codes);
    
    FileBufferIO_close(file_compress);

    archive_write_filesize(archive, size_pos, filesize.compressed, compress_treesize, filestart);

    return filesize;
}

int compress(int paths_c, char** paths, char* archivepath) {
    char* unique_archivepath = generate_unique_filepath(archivepath);
    FileBufferIO* archive = FileBufferIO_open(unique_archivepath, "wb", BUFFER_SIZE);
    if (!archive) {
        free(unique_archivepath);
        return 1;
    }
    CompressingFiles compr_files = prepare_archive(archive, paths_c, paths);

    if (compr_files.files == NULL) {
        free(unique_archivepath);
        FileBufferIO_close(archive);
        return 1;
    } else if (compr_files.files_c == 0) {
        fprintf(stderr, "Empty archive\n");
        free(unique_archivepath);
        freelist(compr_files.files);
        FileBufferIO_close(archive);
        return 1;
    }

    FileSize filesize_total = {.original = 0, .compressed = 0};

    pg_init(compr_files.files_c + compr_files.size * 2, 0);
    llist* cur = compr_files.files->begin;
    while (cur != NULL) {
        FileSize filesize = compress_file(archive, cur->path, cur->size_pos);
        if (filesize.compressed == 0) {
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

    printf("Result: %lld -> %ld bytes\n", filesize_total.original, get_filesize(unique_archivepath));
    printf("Saved in %s\n", unique_archivepath);
    free(unique_archivepath);
    return 0;
}
// == Сжатие файлов ==============================

// == Распаковка файлов ==========================
int create_directories(const char *path) {
    char* pp = NULL;
    char* sp = NULL;

    char* temp = (char*)malloc(strlen(path)+1);
    if (!temp) {
        fprintf(stderr, "Out of memory");
        return 1;
    }
    strncpy(temp, path, sizeof(temp));

    pp = temp;
    while ((sp = strchr(pp, '/')) != NULL) {
        if (sp != temp) { // не корневая директория "/"
            *sp = '\0';
            if (mkdir(temp, 0777) == -1 && errno != EEXIST) {
                perror("mkdir");
                free(temp);
                return 1;
            }
            *sp = '/';
        }
        pp = sp + 1;
    }

    free(temp);
    return 0;
}

int decompress(char* dir, char* archivename) {
    FileBufferIO* archive = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);
    if (!archive) {
        return 1;
    }
    FileBufferIO* archive_frame = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);
    if (!archive_frame) {
        FileBufferIO_close(archive);
        return 1;
    }
    FileFrame fileframe = get_fileframe(archive_frame);
    if (fileframe.count == -1) {
        FileBufferIO_close(archive);
        FileBufferIO_close(archive_frame);
        return 1;
    }

    pg_init(fileframe.count, 0);
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

        char* path = (char*)malloc(strlen(dir) + strlen(fileframe.name) + 2);
        if (!path) {
            pg_end();
            fprintf(stderr, "Out of memory\n");
            end_fileframe(&fileframe);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }
        sprintf(path, "%s/%s", dir, fileframe.name);
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
        unsigned long long compress_size = fileframe.size; // Размер сжатого файла в байтах
        unsigned int compress_treesize = fileframe.treesize;  // Размер дерева в битах

        HuffmanNode* tree = fread_tree(archive, compress_treesize, 0);
        if (!tree) {
            pg_end();
            end_fileframe(&fileframe);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            FileBufferIO_close(file_decompress);
            return 1;
        }

        HuffmanNode* node_cur = tree;
        for (unsigned long long i = 0; i < compress_size - compress_treesize; i++) {
            unsigned char bit = 0;
            if (!archive->readbits(archive, &bit, 7, 1)) {
                pg_end();
                fprintf(stderr, "EOF while decompressing\n");
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                FileBufferIO_close(file_decompress);
                HuffmanNode_freetree(tree);
                return 1;
            }

            if (bit==0) {
                node_cur = node_cur->left;
            } else if (bit==1) {
                node_cur = node_cur->right;
            }

            if (!node_cur) {
                pg_end();
                fprintf(stderr, "Corrupted huffman tree\n");
                end_fileframe(&fileframe);
                FileBufferIO_close(archive);
                FileBufferIO_close(archive_frame);
                FileBufferIO_close(file_decompress);
                HuffmanNode_freetree(tree);
                return 1;
            }
            
            if (node_cur->left==NULL && node_cur->right==NULL) {
                file_decompress->writebytes(file_decompress, &node_cur->byte, 0, sizeof(node_cur->byte));
                node_cur = tree;
            }
        }

        HuffmanNode_freetree(tree);
        FileBufferIO_close(file_decompress);
        pg_update(1);
    } while (next_fileframe(&fileframe, archive_frame));

    FileBufferIO_close(archive);
    FileBufferIO_close(archive_frame);

    pg_end();

    return 0;
}
// == Распаковка файлов ==========================