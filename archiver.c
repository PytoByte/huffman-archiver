// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

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
#include "linkedlist.h"

#define BUFFER_SIZE 4096

static char check_file_exist(char* filepath) {
    struct stat buffer;
    return stat(filepath, &buffer) == 0;
}

static size_t get_filesize(char* filepath) {
    struct stat buffer;
    int exist = stat(filepath, &buffer);
    if (exist != 0) {
        return -1;
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

int digits_count(int num) {
    int c = 0;
    while (num > 0) {
        num /= 10;
        c++;
    }
    return c;
}

long long prepare_file(FileBufferIO* archive, char* path) {
    long long size_pos = 0;

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
        perror("Ошибка stat");
        free(path);
        return 0;
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
            addtolist(list, path, prepare_file(archive, get_filename(startpath)));
        } else {
            char* rootdir = get_filename(startpath);
            char* filepath = (char*)malloc(strlen(rootdir) + strlen(addpath) + 1);
            strcpy(filepath, rootdir);
            strcat(filepath, addpath);
            addtolist(list, path, prepare_file(archive, filepath));
            free(filepath);
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
        perror("Ошибка открытия директории");
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
        stream_read->readbytes(stream_read, &node->byte, 0, sizeof(node->byte));
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
hlist* prepare_archive(FileBufferIO* archive, int paths_c, char** paths) {
    int files_c = 0;
    archive->writebytes(archive, &files_c, 0, sizeof(files_c));

    hlist* list = initlist();

    for (int i = 0; i < paths_c; i++) {
        files_c += prepare_allfiles(archive, list, paths[i], "");
    }
    writebuffer(archive);

    // Обновление количества файлов
    long original_pos = ftell(archive->fp);
    fseek(archive->fp, 0, SEEK_SET);
    fwrite(&files_c, sizeof(files_c), 1, archive->fp);
    fseek(archive->fp, original_pos, SEEK_SET);

    return list;
}

FileFrame get_fileframe(FileBufferIO* archive) {
    FileFrame fileframe;
    fileframe.count = 0;
    fileframe.current = 1;
    fileframe.filestart = 0;
    fileframe.size = 0;
    fileframe.treesize = 0;

    archive->readbytes(archive, &fileframe.count, 0, sizeof(fileframe.count));
    int filename_len = 0;
    archive->readbytes(archive, &filename_len, 0, sizeof(filename_len));
    fileframe.name = (char*)malloc(filename_len);

    archive->readbytes(archive, fileframe.name, 0, filename_len);
    archive->readbytes(archive, &fileframe.size, 0, sizeof(fileframe.size));
    archive->readbytes(archive, &fileframe.treesize, 0, sizeof(fileframe.treesize));
    archive->readbytes(archive, &fileframe.filestart, 0, sizeof(fileframe.filestart));

    return fileframe;
}

char next_fileframe(FileFrame* fileframe, FileBufferIO* archive) {
    free(fileframe->name);
    if (fileframe->current == fileframe->count) {
        return 0;
    }
    fileframe->current++;

    archive->readbytes(archive, &fileframe->count, 0, sizeof(fileframe->count));
    int filename_len = 0;
    archive->readbytes(archive, &filename_len, 0, sizeof(filename_len));
    fileframe->name = (char*)malloc(filename_len);

    archive->readbytes(archive, fileframe->name, 0, filename_len);
    archive->readbytes(archive, &fileframe->size, 0, sizeof(fileframe->size));
    archive->readbytes(archive, &fileframe->treesize, 0, sizeof(fileframe->treesize));
    archive->readbytes(archive, &fileframe->filestart, 0, sizeof(fileframe->filestart));

    return 1;
}

typedef struct {
    unsigned long long original;
    unsigned long long compressed;
} FileSize;


FileSize compress_file(FileBufferIO* archive, char* path, long long size_pos) {
    FileSize filesize;
    filesize.compressed = 0;
    filesize.original = get_filesize(path);
    
    unsigned long long filestart = ftell(archive->fp) * 8;
    filestart += archive->byte_p * 8 + archive->bit_p;
    FileBufferIO* file_compress = FileBufferIO_open(path, "rb", BUFFER_SIZE);

    // Подсчёт частоты символов
    unsigned long long freqs[256] = {0};
    unsigned char byte = 0;
    while (file_compress->readbytes(file_compress, &byte, 0, sizeof(byte))) {
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

    unsigned int compress_treesize = fwrite_tree(tree, archive); // Размер дерева в битах
    filesize.compressed = compress_treesize; // Размер сжатого файла в битах

    HuffmanNode_freetree(tree);

    // Сжатие со спрессовыванием кодов
    while (file_compress->readbytes(file_compress, &byte, 0, sizeof(byte))) {
        filesize.compressed += archive->writebits(archive, codes.codes[byte].code, 0, codes.codes[byte].size);
    }
    Codes_free(codes);
    
    FileBufferIO_close(file_compress);

    archive_write_filesize(archive, size_pos, filesize.compressed, compress_treesize, filestart);

    return filesize;
}

void compress(int paths_c, char** paths, char* archivepath) {
    FileBufferIO* archive = FileBufferIO_open(archivepath, "wb", BUFFER_SIZE);
    hlist* files = prepare_archive(archive, paths_c, paths);

    FileSize filesize_total = {.original = 0, .compressed = 0};

    llist* cur = files->begin;
    while (cur != NULL) {
        FileSize filesize = compress_file(archive, cur->path, cur->size_pos);
        filesize_total.original += filesize.original;
        cur = cur->next;
    }
    freelist(files);

    FileBufferIO_close(archive);

    printf("Result: %lld -> %ld bytes\n", filesize_total.original, get_filesize(archivepath));
    printf("Saved in %s\n", archivepath);
}

int create_directories(const char *path) {
    char *pp = NULL;
    char *sp = NULL;
    char temp[1024];

    strncpy(temp, path, sizeof(temp));
    temp[sizeof(temp)-1] = '\0';

    pp = temp;
    while ((sp = strchr(pp, '/')) != NULL) {
        if (sp != temp) { // не корневая директория "/"
            *sp = '\0';
            if (mkdir(temp, 0777) == -1 && errno != EEXIST) {
                perror("mkdir");
                return -1;
            }
            *sp = '/';
        }
        pp = sp + 1;
    }

    return 0;
}

char* generate_unique_filepath(char* path) {
    // Если файл не существует, возвращаем оригинальный путь
    if (!check_file_exist(path)) {
        return path;
    }

    // Разбираем имя файла и расширение
    char* dot_pos = strrchr(path, '.');
    char* ext = (char*)malloc(strlen(dot_pos)+1);
    strcpy(ext, dot_pos);
    dot_pos[0] = 0;
    char* name = (char*)malloc(strlen(path)+1);
    strcpy(name, path);
    printf("name %s  ext %s\n", name, ext);

    // Генерируем новые имена с добавлением "(n)"
    unsigned int addition_num = 1;
    do {
        path = realloc(path, strlen(name) + strlen(ext) + digits_count(addition_num) + 6);
        if (!path) {
            free(name);
            free(ext);
            free(path);
            return NULL;
        };

        sprintf(path, "%s (%u)%s", name, addition_num, ext);
        addition_num++;
    } while (check_file_exist(path));

    return path;
}

void decompress(char* dir, char* archivename) {
    FileBufferIO* archive = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);
    FileBufferIO* archive_frame = FileBufferIO_open(archivename, "rb", BUFFER_SIZE);
    FileFrame fileframe = get_fileframe(archive_frame);

    do {
        fseek(archive->fp, fileframe.filestart / 8, SEEK_SET);
        nextbuffer(archive);
        archive->bit_p = fileframe.filestart % 8;
        char* path = (char*)malloc(strlen(dir) + strlen(fileframe.name) + 2);
        sprintf(path, "%s/%s", dir, fileframe.name);
        create_directories(path);
        unsigned int addition_num = 0;
        path = generate_unique_filepath(path);
        FileBufferIO* file_decompress = FileBufferIO_open(path, "wb", BUFFER_SIZE);
        free(path);
        unsigned long long compress_size = fileframe.size; // Размер сжатого файла в байтах
        unsigned int compress_treesize = fileframe.treesize;  // Размер дерева в битах

        HuffmanNode* tree = fread_tree(archive);

        HuffmanNode* node_cur = tree;
        for (unsigned long long i = 0; i < compress_size - compress_treesize; i++) {
            unsigned char bit = 0;
            archive->readbits(archive, &bit, 7, 1);

            if (bit==0) {
                node_cur = node_cur->left;
            } else if (bit==1) {
                node_cur = node_cur->right;
            }
            
            if (node_cur->left==NULL && node_cur->right==NULL) {
                file_decompress->writebits(file_decompress, &node_cur->byte, 0, sizeof(node_cur->byte)*8);
                node_cur = tree;
            }
        }

        HuffmanNode_freetree(tree);
        FileBufferIO_close(file_decompress);
    } while (next_fileframe(&fileframe, archive_frame));

    FileBufferIO_close(archive);
    FileBufferIO_close(archive_frame);
}