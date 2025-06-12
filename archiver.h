#pragma once
#include <stdint.h>

#define BUFFER_SIZE 4096

extern uint8_t wordsize;

int compress(char** paths, int paths_count, char* archivepath, uint8_t wordsize_arg);

int decompress(char** paths, int paths_count, char* outdir);