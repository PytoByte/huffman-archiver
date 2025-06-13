#pragma once
#include <stdio.h>

// Returns 1 if the file exists, else 0
int check_file_exist(const char* filepath);

// Returns the size of the file in bytes
size_t get_filesize(const char* filepath);

// Returns a pointer to the file name
char* get_filename(char* filepath);

// Generates a unique filepath by appending "(n)" to the filename
// !!! After use, free returned value if non-null !!!
char* generate_unique_filepath(const char* path);

// Returns 1 if the files similar, else 0
int check_files_similar(char* path1, char* path2);

// Сreates nonexistent directories on the file path
// Returns 0 if success, else 1
int create_directories(const char *path);

