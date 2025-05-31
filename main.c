#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "archiver.h"

void command_help(char* program_name);
void command_compress(int argc, char** argv, char* out_file);
void command_decompress(int argc, char** argv, char* out_dir);

// Вспомогательные функции
char asciitolower(char c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

void strlwr(char* s) {
    for (; *s; s++) *s = asciitolower(*s);
}

bool check_command(const char* command, int alias_count, ...) {
    va_list aliases;
    va_start(aliases, alias_count);
    
    char lower_command[256];
    strncpy(lower_command, command, sizeof(lower_command));
    strlwr(lower_command);
    
    for (int i = 0; i < alias_count; i++) {
        const char* alias = va_arg(aliases, const char*);
        if (strcmp(lower_command, alias) == 0) {
            va_end(aliases);
            return true;
        }
    }
    
    va_end(aliases);
    return false;
}

typedef struct {
    const char* name;
    void (*handler)(int, char**, char*);
    const char* description;
    const char* usage;
} Command;

// Доступные команды
const Command commands[] = {
    {"help", NULL, "Show help information", ""},
    {"compress", command_compress, "Compress files", "file1 [file2...] -o output"},
    {"decompress", command_decompress, "Decompress archive", "archive -o output_dir"},
    {NULL, NULL, NULL, NULL} // Маркер конца
};

void command_help(char* program_name) {
    printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
    printf("Available commands:\n");
    
    for (const Command* cmd = commands; cmd->name; cmd++) {
        printf("  %-10s %s\n", cmd->name, cmd->description);
        if (cmd->usage) printf("    Usage: %s %s %s\n", program_name, cmd->name, cmd->usage);
    }
}

void command_compress(int argc, char** argv, char* out_file) {
    if (argc < 1) {
        fprintf(stderr, "Error: No input files specified\n");
        return;
    }
    
    if (!out_file) out_file = "ar.huf";
    if (compress(argc, argv, out_file) != 0) {
        fprintf(stderr, "Compression canceled\n");
    }
}

void command_decompress(int argc, char** argv, char* out_dir) {
    if (argc < 1) {
        fprintf(stderr, "Error: No input files specified\n");
        return;
    }

    char* archive_file = argv[0];
    if (!out_dir) out_dir = ".";

    if (decompress(out_dir, archive_file) != 0) {
        fprintf(stderr, "Decompression canceled\n");
    }
}

int parse_options(int argc, char** argv, char** out_file) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                *out_file = argv[i + 1];
                return 2; // Пропускаем обработанные аргументы
            } else {
                fprintf(stderr, "Error: Output file not specified\n");
                return -1;
            }
        }
    }
    return 0; // Нет опций для пропуска
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        command_help("huffman");
        return EXIT_FAILURE;
    }

    char* command = argv[1];
    char* out = NULL;
    
    // Пропускаем имя программы и команду
    int new_argc = argc - 2;
    char** new_argv = argv + 2;
    
    // Парсим опции (например, -o)
    int skip = parse_options(new_argc, new_argv, &out);
    if (skip < 0) return EXIT_FAILURE;
    
    // Обновляем аргументы после парсинга опций
    new_argc -= skip;
    
    // Обработка команд
    if (check_command(command, 3, "help", "--help", "-h")) {
        command_help("huffman");
    } 
    else if (check_command(command, 2, "compress", "-c")) {
        command_compress(new_argc, new_argv, out);
    }
    else if (check_command(command, 2, "decompress", "-d")) {
        command_decompress(new_argc, new_argv, out);
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        command_help("huffman");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}