#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "filetools.h"

// Count the digits of a number
static int digits_count(int num) {
    int c = 0;
    while (num > 0) {
        num /= 10;
        c++;
    }
    return c;
}

int check_file_exist(const char* filepath) {
    struct stat buffer;
    return stat(filepath, &buffer) == 0;
}

size_t get_filesize(const char* filepath) {
   struct stat buffer;
   int exist = stat(filepath, &buffer);
   if (exist == -1) {
       perror("stat error");
       return 0;
   }
   return buffer.st_size;
}

char* get_filename(char* filepath) {
   char* filename = strrchr(filepath, '/');
   if (filename == NULL)
       return filepath; // If no separators are found, return the entire filepath

   return filename + 1; // Skip the separator itself
}

char* generate_unique_filepath(const char* path) {
    // Path will be modified, so allocate new memory
   char* unique_path = (char*)malloc(strlen(path)+1);
   strcpy(unique_path, path);

   // If the file does not exist, return the original path
   if (!check_file_exist(unique_path)) {
       return unique_path;
   }

   // Parse the filename and extension
   char* dot_pos = strrchr(get_filename(unique_path), '.');

   char* ext = NULL;
   if (dot_pos) {
       ext = (char*)malloc(strlen(dot_pos)+1);
       if (!ext) {
           fprintf(stderr, "Out of memory\n");
           free(unique_path);
           return NULL;
       }
       strcpy(ext, dot_pos);
       dot_pos[0] = 0;
   }
   
   char* name = (char*)malloc(strlen(unique_path)+1);
   if (!name) {
       fprintf(stderr, "Out of memory\n");
       free(unique_path);
       if (ext) free(ext);
       return NULL;
   }
   strcpy(name, unique_path);

   // Generate new names with "(n)" appended
   unsigned int addition_num = 1;
   do {
       int ext_len = 0;
       if (ext) ext_len = strlen(ext);
       unique_path = realloc(unique_path, strlen(name) + ext_len + digits_count(addition_num) + 6);
       if (!unique_path) {
           fprintf(stderr, "Out of memory\n");
           free(name);
           if (ext) free(ext);
           free(unique_path);
           return NULL;
       };

       if (ext) {
           sprintf(unique_path, "%s (%u)%s", name, addition_num, ext);
       } else {
           sprintf(unique_path, "%s (%u)", name, addition_num);
       }
       addition_num++;
   } while (check_file_exist(unique_path));

   free(name);
   if (ext) free(ext);

   return unique_path;
}

int check_files_similar(char* path1, char* path2) {
    struct stat stat1;
    stat(path1, &stat1);
    struct stat stat2;
    stat(path2, &stat2);

    return stat1.st_ino == stat2.st_ino;
}