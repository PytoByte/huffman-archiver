#include "archiver.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "buffio.h"
#include "progbar.h"
#include "queue.h"
#include "buffio.h"
#include "filetools.h"
#include "huff/tree/builder.h"
#include "huff/tree/codes.h"

uint8_t wordsize = 1;
enum WarningAction compress_warn_act = WARN_ACT_ASK;

typedef struct {
    int count;
    int current;
    char* name;
    uint64_t size_original; // bytes
    uint64_t size_compressed; // bits
    uint32_t treesize; // bits
    uint64_t filestart;
    FileBufferIO* fb;
} HeaderFrame;

// == Result structures ==
typedef struct {
    uint64_t original; // bytes
    uint64_t compressed_bits; // bits
} FileSizeResult;

typedef struct {
    Queue* files;
    unsigned long long total_size; // bits
    int count;
} CompressingFilesResult;

typedef struct {
    char* path;
    long long size_pos;
} CompressingFile;

typedef struct {
    int added;
    unsigned long long filesize; // bits
} PreparedFilesResult;
// == Result structures ==

// create a CompressingFile with the given parameters
// !!! After use, run "CompressingFile_free" if non-null !!!
CompressingFile* CompressingFile_create(char* path, long long size_pos) {
    CompressingFile* file = (CompressingFile*)malloc(sizeof(CompressingFile));
    if (!file) {
        return NULL;
    }

    file->path = (char*)malloc(strlen(path)+1);
    if (!file->path) {
        free(file);
        return NULL;
    }
    strcpy(file->path, path);
    file->size_pos = size_pos;

    return file;
}

void CompressingFile_free(void* ptr) {
    CompressingFile* file = (CompressingFile*)ptr;
    free(file->path);
    free(file);
}

// == Reading and writing a huffman tree (prefix form) ==
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

// Returns tree if successful, NULL if failed
// !!! After use, run "HuffmanNode_freetree" if non-null !!!
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
// == Reading and writing a huffman tree (prefix form) ==



// == Writing headers ============================
static long prepare_fileheader(FileBufferIO* archive, char* filepath, char* path_in_archive) {
    if (strncmp(path_in_archive, "./", 2) == 0) {
        path_in_archive += 2;
    }

    uint32_t filename_len = strlen(path_in_archive)+1;
    uint64_t compressed_filesize = 0;
    uint32_t compressed_treesize = 0;
    uint64_t filestart = 0;

    uint64_t original_filesize = get_filesize(filepath);
    archive->writebytes(archive, &original_filesize, 0, sizeof(original_filesize));
    archive->writebytes(archive, &filename_len, 0, sizeof(filename_len));
    archive->writebytes(archive, path_in_archive, 0, filename_len);

    long size_pos = ftell(archive->fp);
    if (size_pos == -1) return -1;
    size_pos = (size_pos + archive->byte_p) + 1;

    archive->writebytes(archive, &compressed_filesize, 0, sizeof(compressed_filesize));
    archive->writebytes(archive, &compressed_treesize, 0, sizeof(compressed_treesize));
    archive->writebytes(archive, &filestart, 0, sizeof(filestart));

    return size_pos;
}


// startpath - file or dir, which need to compress
// addpath - if startpath if folder, when addpath stores subdirs of start folder
static PreparedFilesResult prepare_headers(FileBufferIO* archive, Queue* queue, char* startpath, char* addpath) {
    char* path = (char*)malloc(strlen(startpath) + strlen(addpath) + 1);
    if (!path) {
        fprintf(stderr, "Out of memory\n");
        return (PreparedFilesResult){-1, 0};
    }

    strcpy(path, startpath);
    strcat(path, addpath);

    if (check_files_similar(archive->path, path)) {
        fprintf(stderr, "WARNING: Skipping archive itself\n");
        free(path);
        return (PreparedFilesResult){0, 0};
    }

    struct stat file_stat;
    if (stat(path, &file_stat) == -1) {
        fprintf(stderr, "Error while opening file %s: ", path);
        perror("stat error");
        free(path);
        return (PreparedFilesResult){-1, 0};
    }

    if (S_ISREG(file_stat.st_mode)) {
        size_t filesize = get_filesize(path);

        // Here is warning if file is too small
        if (compress_warn_act != WARN_ACT_DECLINE) {
            if (filesize < 512) {
                if (compress_warn_act == WARN_ACT_ACCEPT) {
                    free(path);
                    return (PreparedFilesResult){0, 0};
                }

                printf("WARNING \"%s\" (%ld bytes)\n", get_filename(path), filesize);
                printf("File too small (<512 bytes), it may be bigger after compressing\n");
                printf("Skip this file? (y/default n) ");

                char c = getchar();
                if (c == 'y' || c == 'Y') {
                    while ((c = getchar()) != '\n' && c != EOF);
                    free(path);
                    return (PreparedFilesResult){0, 0};
                } else if (c != '\n') {
                    while ((c = getchar()) != '\n' && c != EOF);
                }
            }
        }

        if (strlen(addpath) == 0) { // If just compressing file
            long size_pos = prepare_fileheader(archive, path, get_filename(startpath));
            if (size_pos == -1) {
                free(path);
                fprintf(stderr, "Getting file position error\n");
                return (PreparedFilesResult){-1, 0};
            }
            CompressingFile* compr_file = CompressingFile_create(path, size_pos);
            if (!compr_file) {
                free(path);
                fprintf(stderr, "Out of memory\n");
                return (PreparedFilesResult){-1, 0};
            }
            if (queue_enqueue(queue, compr_file)) {
                fprintf(stderr, "Out of memory\n");
                free(path);
                CompressingFile_free(compr_file);
                return (PreparedFilesResult){-1, 0};
            };
        } else { // If compressing file in compressing folder
            char* rootdir = get_filename(startpath);
            char* filepath = (char*)malloc(strlen(rootdir) + strlen(addpath) + 1);
            if (!filepath) {
                fprintf(stderr, "Out of memory\n");
                free(path);
                return (PreparedFilesResult){-1, 0};
            }
            strcpy(filepath, rootdir);
            strcat(filepath, addpath);
            long size_pos = prepare_fileheader(archive, path, filepath);
            free(filepath);
            if (size_pos == -1) {
                free(path);
                fprintf(stderr, "Getting file position error\n");
                return (PreparedFilesResult){-1, 0};
            }
            CompressingFile* compr_file = CompressingFile_create(path, size_pos);
            if (!compr_file) {
                free(path);
                fprintf(stderr, "Out of memory\n");
                return (PreparedFilesResult){-1, 0};
            }
            if (queue_enqueue(queue, compr_file)) {
                fprintf(stderr, "Out of memory\n");
                free(path);
                CompressingFile_free(compr_file);
                return (PreparedFilesResult){-1, 0};
            };
        }
        free(path);
        return (PreparedFilesResult){1, filesize};
    }

    if (!S_ISDIR(file_stat.st_mode)) {
        pg_end();
        fprintf(stderr, "Unsupported file \"%s\"\n", path);
        free(path);
        return (PreparedFilesResult){0, 0};
    }

    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "Error while opening dir %s: ", path);
        perror("Open dir error");
        free(path);
        return (PreparedFilesResult){0, 0};
    }

    

    PreparedFilesResult files = {0, 0};
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char* new_addpath = (char*)malloc(strlen(addpath) + strlen(entry->d_name) + 2);
        sprintf(new_addpath, "%s/%s", addpath, entry->d_name);

        PreparedFilesResult temp = prepare_headers(archive, queue, startpath, new_addpath);
        files.added += temp.added;
        files.filesize += temp.filesize;

        free(new_addpath);
    }

    free(path);
    closedir(dir);

    return files;
}

static void fwrite_compressed_filesize(FileBufferIO* archive, long size_pos, uint64_t filesize, uint32_t treesize, uint64_t filestart) {
    writebuffer(archive);
    long original_pos = ftell(archive->fp);
    fseek(archive->fp, size_pos, SEEK_SET);
    archive->writebytes(archive, &filesize, 0, sizeof(filesize));
    archive->writebytes(archive, &treesize, 0, sizeof(treesize));
    archive->writebytes(archive, &filestart, 0, sizeof(filestart));
    writebuffer(archive);
    fseek(archive->fp, original_pos, SEEK_SET);
}

// Reserves space for headers at the beginning of the archive
// Returns a list of files and pointers for compression and header filling
static CompressingFilesResult prepare_archive(FileBufferIO* archive, int paths_c, char** paths) {
    CompressingFilesResult compr_files;
    
    compr_files.files = queue_create();
    compr_files.count = 0;
    compr_files.total_size = 0;
    archive->writebytes(archive, &compr_files.count, 0, sizeof(compr_files.count));
    archive->writebytes(archive, &wordsize, 0, sizeof(wordsize));

    for (int i = 0; i < paths_c; i++) {
        PreparedFilesResult temp = prepare_headers(archive, compr_files.files, paths[i], "");
        if (temp.added == -1) {
            queue_destroy(&compr_files.files, CompressingFile_free);
            compr_files.count = 0;
            return compr_files;
        }
        compr_files.count += temp.added;
        compr_files.total_size += temp.filesize;
    }
    writebuffer(archive);

    // Updating the number of files
    long original_pos = ftell(archive->fp);
    if (original_pos == -1) {
        fprintf(stderr, "Getting file position error\n");
        queue_destroy(&compr_files.files, CompressingFile_free);
        compr_files.count = 0;
        return compr_files;
    }
    
    if (fseek(archive->fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Fseek error\n");
        queue_destroy(&compr_files.files, CompressingFile_free);
        compr_files.count = 0;
        return compr_files;
    }

    writebuffer(archive);
    archive->writebytes(archive, &compr_files.count, 0, sizeof(compr_files.count));
    writebuffer(archive);

    if (fseek(archive->fp, original_pos, SEEK_SET) != 0) {
        fprintf(stderr, "Fseek error\n");
        queue_destroy(&compr_files.files, CompressingFile_free);
        compr_files.count = 0;
        return compr_files;
    }

    return compr_files;
}
// == Writing headers ============================


// == HeaderFrame ================================
static void end_header_frame(HeaderFrame* header_frame) {
    free(header_frame->name);
    header_frame->current = header_frame->count;
}

static HeaderFrame get_header_frame(FileBufferIO* archive, int count) {
    HeaderFrame header_frame;
    header_frame.fb = archive;
    header_frame.count = count;
    header_frame.current = 1;
    header_frame.filestart = 0;
    header_frame.size_compressed = 0;
    header_frame.size_original = 0;
    header_frame.treesize = 0;

    if (!archive->readbytes(archive, &header_frame.size_original, 0, sizeof(header_frame.size_original))) {
        fprintf(stderr, "EOF while reading headers\n");
        header_frame.count = -1;
        return header_frame;
    }

    int filename_len = 0;
    if (!archive->readbytes(archive, &filename_len, 0, sizeof(filename_len))) {
        fprintf(stderr, "EOF while reading headers\n");
        header_frame.count = -1;
        return header_frame;
    }

    header_frame.name = (char*)malloc(filename_len);
    if (!header_frame.name) {
        fprintf(stderr, "Out of memory\n");
        header_frame.count = -1;
        return header_frame;
    }

    if (!archive->readbytes(archive, header_frame.name, 0, filename_len)) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(&header_frame);
        header_frame.count = -1;
        return header_frame;
    }
    if (!archive->readbytes(archive, &header_frame.size_compressed, 0, sizeof(header_frame.size_compressed))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(&header_frame);
        header_frame.count = -1;
        return header_frame;
    }
    if (!archive->readbytes(archive, &header_frame.treesize, 0, sizeof(header_frame.treesize))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(&header_frame);
        header_frame.count = -1;
        return header_frame;
    }
    if (!archive->readbytes(archive, &header_frame.filestart, 0, sizeof(header_frame.filestart))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(&header_frame);
        header_frame.count = -1;
        return header_frame;
    }

    return header_frame;
}

static int next_header_frame(HeaderFrame* header_frame) {
    FileBufferIO* archive = header_frame->fb;

    free(header_frame->name);
    if (header_frame->current == header_frame->count) {
        return 0;
    }
    header_frame->current++;

    if (!archive->readbytes(archive, &header_frame->size_original, 0, sizeof(header_frame->size_original))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(header_frame);
        return 0;
    }

    unsigned int filename_len = 0;
    archive->readbytes(archive, &filename_len, 0, sizeof(filename_len));
    header_frame->name = (char*)calloc(filename_len, 1);
    if (header_frame->name == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }

    if (!archive->readbytes(archive, header_frame->name, 0, filename_len)) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(header_frame);
        return 0;
    }
    header_frame->size_compressed = 0;
    if (!archive->readbytes(archive, &header_frame->size_compressed, 0, sizeof(header_frame->size_compressed))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(header_frame);
        return 0;
    }
    header_frame->treesize = 0;
    if (!archive->readbytes(archive, &header_frame->treesize, 0, sizeof(header_frame->treesize))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(header_frame);
        return 0;
    }
    header_frame->filestart = 0;
    if (!archive->readbytes(archive, &header_frame->filestart, 0, sizeof(header_frame->filestart))) {
        fprintf(stderr, "EOF while reading headers\n");
        end_header_frame(header_frame);
        return 0;
    }

    return 1;
}
// == HeaderFrame ================================


// == Files compression ==========================
static FileSizeResult compress_file(FileBufferIO* archive, const char* path, long size_pos) {
    FileSizeResult filesize;
    filesize.compressed_bits = 0;
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

    // Calculating words frequency
    unsigned int freqs_size = (1 << (wordsize*8));
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
            for (int i = 0; i < wordsize; i++) {
                lastword[i] = word[i];
            }
            lastword_size = readed;
        } else {
            freqs[wordtoi(word)]++;
        }
        pg_update(readed);
    }

    // Building a tree
    TreeBuilder* tree_builder = TreeBuilder_create(freqs_size+1);
    for (unsigned int i = 0; i < freqs_size; i++) {
        if (freqs[i]==0) {
            continue;
        }
        itoword(i, word);
        
        HuffmanNode* node = HuffmanNode_create(wordsize*8, word, freqs[i], NULL, NULL);
        if (!node) {
            fprintf(stderr, "Out of memory\n");
            FileBufferIO_close(file_compress);
            free(freqs);
            free(word);
            free(lastword);
            TreeBuilder_free(tree_builder);
            return filesize;
        }

        tree_builder->insert(tree_builder, node);
    }

    if (lastword) {
        HuffmanNode* node = HuffmanNode_create(lastword_size, lastword, 1, NULL, NULL);
        if (!node) {
            fprintf(stderr, "Out of memory\n");
            FileBufferIO_close(file_compress);
            free(freqs);
            free(word);
            free(lastword);
            TreeBuilder_free(tree_builder);
            return filesize;
        }
        tree_builder->insert(tree_builder, node);
        free(lastword);
    }

    free(freqs);

    HuffmanNode* tree = tree_builder->extract_tree(tree_builder);

    TreeBuilder_free(tree_builder);

    // Tree encoding
    Codes codes = Codes_build(tree);
    if (codes.size == 0) {
        free(word);
        FileBufferIO_close(file_compress);
        return filesize;
    }

    // File compression
    rewind(file_compress->fp);

    uint32_t compress_treesize = fwrite_tree(tree, archive); // Tree size in bits
    filesize.compressed_bits = compress_treesize; // Compressed file size in bits

    HuffmanNode_freetree(tree);

    // Compression with code compression
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
        filesize.compressed_bits += archive->writebits(archive, code, 0, size);
        pg_update(readed);
    }
    free(word);
    Codes_free(codes);
    
    FileBufferIO_close(file_compress);

    fwrite_compressed_filesize(archive, size_pos, filesize.compressed_bits, compress_treesize, filestart);

    return filesize;
}

int compress(char** paths, int paths_count, char* archivepath) {
    // Сhecking that the paths have been passed
    if (paths_count <= 0) {
        fprintf(stderr, "Nothing to compress\n");
        return 1;
    }

    // Generate unique archive path
    char* unique_archivepath = generate_unique_filepath(archivepath);
    FileBufferIO* archive = FileBufferIO_open(unique_archivepath, "wb", BUFFER_SIZE);
    if (!archive) {
        free(unique_archivepath);
        return 1;
    }
    
    // Prepare archive headers
    printf("Preparing headers...\n");
    CompressingFilesResult compr_files = prepare_archive(archive, paths_count, paths); 
    if (compr_files.files == NULL) {
        free(unique_archivepath);
        FileBufferIO_close_remove(archive);
        return 1;
    } else if (compr_files.count == 0) {
        fprintf(stderr, "Nothing to compress\n");
        free(unique_archivepath);
        queue_destroy(&compr_files.files, CompressingFile_free);
        FileBufferIO_close_remove(archive);
        return 1;
    }

    FileSizeResult filesize_total = {.original = 0, .compressed_bits = 0};

    printf("Compressing %d files...\n", compr_files.count);
    pg_init(compr_files.count + compr_files.total_size*16, 0);

    CompressingFile* compr_file = (CompressingFile*)queue_dequeue(compr_files.files);
    while (compr_file != NULL) {
        // Сhecking if file is archive
        if (strcmp(get_filename(compr_file->path), unique_archivepath) == 0) {
            CompressingFile_free(compr_file);
            continue;
        }
        FileSizeResult filesize = compress_file(archive, compr_file->path, compr_file->size_pos);
        if (filesize.compressed_bits == 0 && filesize.original != 0) {
            pg_end();
            fprintf(stderr, "Error while compressing %s\n", compr_file->path);
            free(unique_archivepath);
            CompressingFile_free(compr_file);
            queue_destroy(&compr_files.files, CompressingFile_free);
            FileBufferIO_close_remove(archive);
            return 1;
        }
        filesize_total.original += filesize.original;
        pg_update(1);

        CompressingFile_free(compr_file);
        compr_file = (CompressingFile*)queue_dequeue(compr_files.files);
    }
    queue_destroy(&compr_files.files, CompressingFile_free);
    pg_end();

    FileBufferIO_close(archive);

    size_t archive_size = get_filesize(unique_archivepath);
    printf("Result: %ld -> %ld bytes (k=%.3lf)\n", filesize_total.original, archive_size, (double)archive_size/filesize_total.original);
    printf("Saved in %s\n", unique_archivepath);
    free(unique_archivepath);
    return 0;
}
// == Files compression ==========================

int decompress(char* archivepath, char* outdir, char** filepaths, int filepaths_count, char** dirpaths, int dirpaths_count) {
    if (!archivepath) {
        fprintf(stderr, "Nothing to decompress\n");
        return 1;
    }

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
        fprintf(stderr, "Corrupted file: EOF while reading headers\n");
        FileBufferIO_close(archive);
        FileBufferIO_close(archive_frame);
        return 1;
    }

    if (!archive_frame->readbytes(archive_frame, &wordsize, 0, sizeof(wordsize))) {
        fprintf(stderr, "Corrupted file: EOF while reading headers\n");
        FileBufferIO_close(archive);
        FileBufferIO_close(archive_frame);
        return 1;
    }

    if (wordsize == 0 || wordsize > 2) {
        fprintf(stderr, "Corrupted file: invalid wordsize\n");
        FileBufferIO_close(archive);
        FileBufferIO_close(archive_frame);
        return 1;
    }

    HeaderFrame header_frame = get_header_frame(archive_frame, files_count);
    if (header_frame.count == -1) {
        FileBufferIO_close(archive);
        FileBufferIO_close(archive_frame);
        return 1;
    }

    printf("Decompressing files from %s\n", archivepath);
    pg_init(get_filesize(archivepath)*8, 0);

    int decompressed_count = 0;
    int filepaths_remain = filepaths_count;
    int dirpaths_remain = dirpaths_count;
    int lastdir_ind = -1;
    do {
        // cutted_filepath pointer may be moved forward, to cut non-required directories
        char* cutted_filepath = header_frame.name;

        // If filter is set
        if (filepaths_count+dirpaths_count > 0) {
            int flag_match = 0;

            if (lastdir_ind >= 0 && strstr(header_frame.name, dirpaths[lastdir_ind]) == header_frame.name) {
                flag_match = 1;
                
                cutted_filepath += get_filename(dirpaths[lastdir_ind])-dirpaths[lastdir_ind];
            } else {
                lastdir_ind = -1;
            }

            // check dir match
            for (int i = 0; i < dirpaths_count && dirpaths_remain > 0 && !flag_match; i++) {
                if (strstr(header_frame.name, dirpaths[i]) == header_frame.name) {
                    flag_match = 1;
                    lastdir_ind = i;
                    dirpaths_remain--;

                    cutted_filepath += get_filename(dirpaths[i])-dirpaths[i];

                    break;
                }
            }

            // check file match
            // if file found in required directory earlier, it skips
            for (int i = 0; i < filepaths_count && filepaths_remain > 0 && !flag_match; i++) {
                if (strstr(header_frame.name, filepaths[i]) != header_frame.name) {
                    continue;
                }

                if (strcmp(get_filename(header_frame.name), get_filename(filepaths[i])) == 0) {
                    flag_match = 1;
                    filepaths_remain--;
                    cutted_filepath = get_filename(cutted_filepath);
                    break;
                }
            }

            if (!flag_match) continue;
        }

        int fs = fseek(archive->fp, header_frame.filestart / 8, SEEK_SET);
        if (fs != 0) {
            pg_end();
            fprintf(stderr, "Fseek error\n");
            end_header_frame(&header_frame);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }

        nextbuffer(archive);
        archive->bit_p = header_frame.filestart % 8;

        char* path = (char*)malloc(strlen(outdir) + strlen(cutted_filepath) + 2);
        if (!path) {
            pg_end();
            fprintf(stderr, "Out of memory\n");
            end_header_frame(&header_frame);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }
        sprintf(path, "%s/%s", outdir, cutted_filepath);
        if (create_directories(path) != 0) {
            pg_end();
            free(path);
            end_header_frame(&header_frame);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }

        char* unique_path = generate_unique_filepath(path);
        free(path);
        if (!unique_path) {
            pg_end();
            end_header_frame(&header_frame);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }
        FileBufferIO* file_decompress = FileBufferIO_open(unique_path, "wb", BUFFER_SIZE);
        if (!file_decompress) {
            pg_end();
            free(unique_path);
            end_header_frame(&header_frame);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            return 1;
        }
        free(unique_path);

        if (header_frame.treesize == 0) {
            FileBufferIO_close(file_decompress);
            continue;
        }

        HuffmanNode* tree = fread_tree(archive, header_frame.treesize, 0);
        if (!tree) {
            pg_end();
            end_header_frame(&header_frame);
            FileBufferIO_close(archive);
            FileBufferIO_close(archive_frame);
            FileBufferIO_close(file_decompress);
            return 1;
        }

        HuffmanNode* node_cur = tree;
        for (unsigned long long i = 0; i < header_frame.size_compressed - header_frame.treesize; i++) {
            unsigned char bit = 0;
            size_t readed = 0;
            if (!(readed = archive->readbits(archive, &bit, 7, 1))) {
                pg_end();
                fprintf(stderr, "EOF while decompressing\n");
                end_header_frame(&header_frame);
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
                    end_header_frame(&header_frame);
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
                end_header_frame(&header_frame);
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
        decompressed_count++;
    } while (next_header_frame(&header_frame));

    FileBufferIO_close(archive);
    FileBufferIO_close(archive_frame);

    pg_end();
    printf("Decompressed %d files in %s\n", decompressed_count, outdir);

    return 0;
}

int show_files(char* archivepath, char* dirpath) {
    char* dirpath_files = NULL;
    if (dirpath) {
        dirpath_files = (char*)malloc(strlen(dirpath)+2);
        strncpy(dirpath_files, dirpath, strlen(dirpath)+2);
        dirpath_files[strlen(dirpath)] = '/';
    } else {
        dirpath_files = (char*)malloc(1);
        dirpath_files[0] = '\0';
    }

    FileBufferIO* archive = FileBufferIO_open(archivepath, "rb", BUFFER_SIZE);
    if (!archive) {
        free(dirpath_files);
        return 1;
    }

    uint32_t files_count = 0;
    if (!archive->readbytes(archive, &files_count, 0, sizeof(files_count))) {
        fprintf(stderr, "EOF while reading headers\n");
        free(dirpath_files);
        FileBufferIO_close(archive);
        return 1;
    }

    if (!archive->readbytes(archive, &wordsize, 0, sizeof(wordsize))) {
        fprintf(stderr, "EOF while reading headers\n");
        free(dirpath_files);
        FileBufferIO_close(archive);
        return 1;
    }

    HeaderFrame header_frame = get_header_frame(archive, files_count);
    if (header_frame.count == -1) {
        free(dirpath_files);
        FileBufferIO_close(archive);
        return 1;
    }

    char empty_dir = 1;
    char* prevdir = NULL;
    do {
        if (strstr(header_frame.name, dirpath_files) != header_frame.name) {
            continue;
        }

        if (empty_dir) {
            printf("Preview archive: \"%s\"\n", archivepath);
            printf("Directory: \"%s\"\n", dirpath ? dirpath : "");
            empty_dir = 0;
        }

        char* filename = header_frame.name + strlen(dirpath_files);
        char* bs = strchr(filename, '/');
        if (bs) {
            filename[bs-filename] = '\0';
            
            if (prevdir) {
                if (strcmp(prevdir, filename) == 0) {
                    continue;
                } else {
                    free(prevdir);
                    prevdir = NULL;
                }
            }
            
            if (!prevdir) {
                prevdir = (char*)malloc(strlen(filename)+1);
                strcpy(prevdir, filename);
            }

            printf("<DIR>  ");
        } else {
            printf("<FILE> ");
        }

        if (bs) {
            printf("          %s\n", filename);
            continue;
        }

        uint64_t filesize = header_frame.size_original;
        char sizename = ' ';
        if (filesize >= 1024) {
            sizename = 'K';
            filesize /= 1024;
        }
        if (filesize >= 1024) {
            sizename = 'M';
            filesize /= 1024;
        }
        if (filesize >= 1024) {
            sizename = 'G';
            filesize /= 1024;
        }

        if (sizename == ' ') {
            printf("(%-4ld  B) %s\n", filesize, filename);
        } else {
            printf("(%-4ld %cB) %s\n", filesize, sizename, filename);
        }
    } while (next_header_frame(&header_frame));

    if (empty_dir) {
        printf("No such directory \"%s\" in \"%s\"\n", dirpath, archivepath);
    }

    if (prevdir) {
        free(prevdir);
    }

    free(dirpath_files);
    FileBufferIO_close(archive);

    return 0;
}