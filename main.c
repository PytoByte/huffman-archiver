#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "archiver.h"

enum CommandType {
    HELP = 0,
    COMPRESS = 1,
    DECOMPRESS = 2,
    INVALID_COMMAND,
    PARSER_ERROR = -1
};

enum OptionType {
    OPTION_OUTPUT = 0,
    OPTION_WORDSIZE = 1,
    INVALID_OPTION
};

typedef struct {
    enum CommandType cmd;
    char* out;
    char** files;
    int files_count;
    int wordsize;
} Instruction;

typedef struct Manual {
    const int aliases_count;
    const char** aliases;
    const char* description;
    const char* usage;
} Manual;

Manual commands_manual[] = {
    {2, (const char*[]){"-help", "-h"}, "Show help information", "-help"},
    {2, (const char*[]){"-compress", "-c"}, "Compress files", "-compress [files|dirs] -output <file>"},
    {2, (const char*[]){"-decompress", "-d"}, "Decompress files", "-decompress [archives] -output <dir>"},
    {0, NULL, NULL, NULL}
};

Manual options_manual[] = {
    {2, (const char*[]){"-output", "-o"}, "Specify output file or directory", "-output <file|dir>"},
    {2, (const char*[]){"-word", "-w"}, "Specify word size in bytes (from 1 to 3)", "-word <number>"},
    {0, NULL, NULL, NULL}
};

void command_help(char* program_name) {
    printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
    printf("Commands:\n");
    for (int i = 0; commands_manual[i].aliases_count != 0; i++) {
        printf("  ");
        for (int j = 0; j < commands_manual[i].aliases_count; j++) {
            printf("%s ", commands_manual[i].aliases[j]);
        }
        printf(" %s\n", commands_manual[i].description);
        printf("    Usage: %s %s\n", program_name, commands_manual[i].usage);
        printf("\n");
    }

    printf("Options:\n");
    for (int i = 0; options_manual[i].aliases_count != 0; i++) {
        printf("  ");
        for (int j = 0; j < options_manual[i].aliases_count; j++) {
            printf("%s ", options_manual[i].aliases[j]);
        }
        printf(" %s\n", options_manual[i].description);
        printf("    Usage: %s\n", options_manual[i].usage);
        printf("\n");
    }
}

char asciitolower(char c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

void strlwr(char* s) {
    for (; *s; s++) *s = asciitolower(*s);
}

int check_flag(const char* flag, const char** aliases, int aliases_count) {
    char* lower_command = (char*)malloc(strlen(flag) + 1);
    if (!lower_command) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }
    strcpy(lower_command, flag);
    strlwr(lower_command);

    for (int i = 0; i < aliases_count; i++) {
        if (strcmp(lower_command, aliases[i]) == 0) {
            free(lower_command);
            return 1;
        }
    }
    free(lower_command);
    return 0;
}

Instruction parse_instruction(int argc, char** argv) {
    Instruction instruction = {INVALID_COMMAND, NULL, NULL, 0, 1};

    instruction.files = (char**)malloc(argc * sizeof(char*));
    if (!instruction.files) {
        instruction.cmd = PARSER_ERROR;
        fprintf(stderr, "Memory allocation failed\n");
        return instruction;
    }

    int check = 0;
    for (int i = 0; i < argc; i++) {
        check = check_flag(argv[i], commands_manual[HELP].aliases, commands_manual[HELP].aliases_count);
        if (check == 1) {
            instruction.cmd = HELP;
            return instruction;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free(instruction.files);
            return instruction;
        }

        check = check_flag(argv[i], commands_manual[COMPRESS].aliases, commands_manual[COMPRESS].aliases_count);
        if (check == 1) {
            if (instruction.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                instruction.cmd = PARSER_ERROR;
                free(instruction.files);
                return instruction;
            }
            instruction.cmd = COMPRESS;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free(instruction.files);
            return instruction;
        }

        check = check_flag(argv[i], commands_manual[DECOMPRESS].aliases, commands_manual[DECOMPRESS].aliases_count);
        if (check == 1) {
            instruction.cmd = DECOMPRESS;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free(instruction.files);
            return instruction;
        }

        check = check_flag(argv[i], options_manual[OPTION_OUTPUT].aliases, options_manual[OPTION_OUTPUT].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option %s requires an arguments\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free(instruction.files);
                return instruction;
            }
            instruction.out = argv[i+1];
            i += 1;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free(instruction.files);
            return instruction;
        }

        check = check_flag(argv[i], options_manual[OPTION_WORDSIZE].aliases, options_manual[OPTION_WORDSIZE].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option %s requires an arguments\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free(instruction.files);
                return instruction;
            }

            int parse_wordsize = atoi(argv[i+1]);
            if (parse_wordsize < 1 || parse_wordsize>3) {
                fprintf(stderr, "Option %s can take only one of the following values: from 1 to 3\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free(instruction.files);
                return instruction;
            }

            instruction.wordsize = parse_wordsize;
            i += 1;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free(instruction.files);
            return instruction;
        }

        instruction.files[instruction.files_count++] = argv[i];
    }

    return instruction;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        command_help(argv[0]);
        return 1;
    }

    Instruction ins = parse_instruction(argc-1, argv+1);

    if (ins.cmd == PARSER_ERROR) {
        return 1;
    }

    if (ins.cmd == HELP) {
        command_help(argv[0]);
    } else if (ins.cmd == COMPRESS) {
        int flag;
        if (!ins.out) {
            flag = compress(ins.files, ins.files_count, "archive.huff", ins.wordsize);
        } else {
            flag = compress(ins.files, ins.files_count, ins.out, ins.wordsize);
        }

        if (flag != 0) {
            fprintf(stderr, "Compression canceled\n");
            free(ins.files);
            return 1;
        }
    } else if (ins.cmd == DECOMPRESS) {
        int flag;
        if (!ins.out) {
            flag = decompress(ins.files, ins.files_count, ".", ins.wordsize);
        } else {
            flag = decompress(ins.files, ins.files_count, ins.out, ins.wordsize);
        }

        if (flag) {
            fprintf(stderr, "Decompression canceled\n");
            free(ins.files);
            return 1;
        }
    } else {
        fprintf(stderr, "Invalid command, check avaiable commands with:\n");
        fprintf(stderr, "  %s -help\n", argv[0]);
        free(ins.files);
        return 1;
    }

    free(ins.files);
    return 0;
}