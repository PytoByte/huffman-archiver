#pragma once
#include <stdint.h>

#define BUFFER_SIZE 4096

// Size of words in bytes to compress/decompress
extern uint8_t wordsize;

// Skip warning about small files
extern int add_small_files;

// Compress files from paths and store them into archive on archivepath
// returns 0 if success, else 1
int compress(char** paths, int paths_count, char* archivepath);

// Decompress archives from paths into outdir
// archivefile is directory if it ends with '/'
// returns 0 if success, else 1
int decompress(char* archivepath, char* outdir, char** filepaths, int filepaths_count, char** dirpaths, int dirpaths_count);

// Show files in archive on archivepath in directory dirpath
// returns 0 if success, else 1
int show_files(char* archivepath, char* dirpath);