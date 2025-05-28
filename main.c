#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "archiver.h"

void command_help(int argc, char** argv) {
    printf("Usage: huffman [command] ...\n");
    printf("brbrbruh\n");
}

void command_compress(int argc, char** argv) {
    compress(argc, argv, "archive.huff");
}

void command_decompress(int argc, char** argv) {
    decompress("out/", "archive.huff");
}

char asciitolower(char c) {
    if (c <= 'Z' && c >= 'A')
        return c - ('Z' - 'z');
    return c;
}

char* strlwr(char* s) {
    for (int i = 0; i < strlen(s); i++) {
        s[i] = asciitolower(s[i]);
    }
    return s;
}

char check_command(char* command, int alias_count, ...) {
    char res = 0;

    va_list aliases;
    va_start(aliases, alias_count);

    for (int i = 0; i < alias_count; i++) {
        char* alias = va_arg(aliases, char*);
        if (strcmp(strlwr(command), alias)==0) {
            res = 1;
            break;
        }
    }

    va_end(aliases);

    return res;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        command_help(0, NULL);
        return 0;
    }

    char* command = argv[1];
    int params_c = argc-2;
    char** params = NULL;
    if (params_c>0) params = &argv[2];

    if (check_command(command, 2, "--h", "--help")) {
        command_help(params_c, params);
    } else if (check_command(command, 1, "-c")) {
        command_compress(params_c, params);
    } else if (check_command(command, 1, "-d")) {
        command_decompress(params_c, params);
    } else {
        command_help(0, NULL);
    }

    return 0;
}