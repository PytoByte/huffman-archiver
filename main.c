#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archiver.h"

enum CommandType {
    HELP = 0,
    COMPRESS = 1,
    DECOMPRESS = 2,
    LIST = 3,
    INVALID_COMMAND,
    PARSER_ERROR = -1
};

enum OptionType {
    OPTION_OUTPUT = 0,
    OPTION_WORDSIZE = 1,
    OPTION_DIR = 2,
    OPTION_SKIPWARNING = 3,
    INVALID_OPTION
};

typedef struct {
    enum CommandType cmd;
    char* archive;
    char* out;
    char** files;
    int files_count;
    char** dirs;
    int dirs_count;
    int wordsize;
    int skip_warning;
} Instruction;

typedef struct Manual {
    const int aliases_count;
    const char** aliases;
    const char* description;
    const char* usage;
} Manual;

Manual commands_manual[] = {
    {2, (const char*[]){"-help", "-h"}, "Show help information", "-help"},
    {2, (const char*[]){"-compress", "-c"}, "Compress files", "-compress [files|dirs] -output <file> -word <number>"},
    {2, (const char*[]){"-decompress", "-d"}, "Decompress files", "-decompress <archive> -output <dir> [files] [-dir <path>]"},
    {2, (const char*[]){"-list", "-ls"}, "Show list of files in archive. Use -dir to select dir in archive", "-list <archive> -dir <path>"},
    {0, NULL, NULL, NULL}
};

Manual options_manual[] = {
    {2, (const char*[]){"-output", "-o"}, "Specify path for command", "-output <file|dir>"},
    {2, (const char*[]){"-word", "-w"}, "Specify word size in bytes (from 1 to 3)", "-word <number>"},
    {1, (const char*[]){"-dir"}, "Specify directory inside archive", "-dir <path>"},
    {2, (const char*[]){"-skipwarning", "-sw"}, "Skip warning about small files", "-skipwarning"},
    {0, NULL, NULL, NULL}
};

void command_help(char* program_name) {
    printf("Commands:\n");
    for (int i = 0; commands_manual[i].aliases_count != 0; i++) {
        printf("  ");
        for (int j = 0; j < commands_manual[i].aliases_count; j++) {
            printf("%s ", commands_manual[i].aliases[j]);
        }
        printf("\n");
        printf("    Description: %s\n", commands_manual[i].description);
        printf("    Usage: %s %s\n", program_name, commands_manual[i].usage);
        printf("\n");
    }

    printf("Options:\n");
    for (int i = 0; options_manual[i].aliases_count != 0; i++) {
        printf("  ");
        for (int j = 0; j < options_manual[i].aliases_count; j++) {
            printf("%s ", options_manual[i].aliases[j]);
        }
        printf("\n");
        printf("    Description: %s\n", options_manual[i].description);
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

void free_instruction(Instruction ins) {
    if (ins.dirs) {
        free(ins.dirs);
    }

    if (ins.files) {
        free(ins.files);
    }
}

Instruction parse_instruction(int argc, char** argv) {
    Instruction instruction = {INVALID_COMMAND, NULL, NULL, NULL, 0, NULL, 0, 1, 0};

    instruction.files = (char**)malloc(argc * sizeof(char*));
    if (!instruction.files) {
        instruction.cmd = PARSER_ERROR;
        fprintf(stderr, "Memory allocation failed\n");
        return instruction;
    }

    instruction.dirs = (char**)malloc((argc/2) * sizeof(char*));
    if (!instruction.files) {
        instruction.cmd = PARSER_ERROR;
        fprintf(stderr, "Memory allocation failed\n");
        free(instruction.files);
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
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], commands_manual[COMPRESS].aliases, commands_manual[COMPRESS].aliases_count);
        if (check == 1) {
            if (instruction.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }
            instruction.cmd = COMPRESS;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], commands_manual[DECOMPRESS].aliases, commands_manual[DECOMPRESS].aliases_count);
        if (check == 1) {
            if (instruction.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }

            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }

            instruction.archive = argv[i+1];
            i += 1;

            instruction.cmd = DECOMPRESS;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], commands_manual[LIST].aliases, commands_manual[LIST].aliases_count);
        if (check == 1) {
            if (instruction.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }
            instruction.cmd = LIST;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], options_manual[OPTION_OUTPUT].aliases, options_manual[OPTION_OUTPUT].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }
            instruction.out = argv[i+1];
            i += 1;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], options_manual[OPTION_WORDSIZE].aliases, options_manual[OPTION_WORDSIZE].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }

            int parse_wordsize = atoi(argv[i+1]);
            if (parse_wordsize < 1 || parse_wordsize>3) {
                fprintf(stderr, "Option \"%s\" can take only one of the following values: from 1 to 3\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }

            instruction.wordsize = parse_wordsize;
            i += 1;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], options_manual[OPTION_DIR].aliases, options_manual[OPTION_DIR].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                instruction.cmd = PARSER_ERROR;
                free_instruction(instruction);
                return instruction;
            }
            instruction.dirs[instruction.dirs_count++] = argv[i+1];
            i += 1;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
            return instruction;
        }

        check = check_flag(argv[i], options_manual[OPTION_SKIPWARNING].aliases, options_manual[OPTION_SKIPWARNING].aliases_count);
        if (check == 1) {
            instruction.skip_warning = 1;
            continue;
        } else if (check == -1) {
            instruction.cmd = PARSER_ERROR;
            free_instruction(instruction);
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
        wordsize = ins.wordsize;
        add_small_files = ins.skip_warning;

        int flag;
        if (!ins.out) {
            flag = compress(ins.files, ins.files_count, "archive.huff");
        } else {
            flag = compress(ins.files, ins.files_count, ins.out);
        }

        if (flag != 0) {
            fprintf(stderr, "Compression canceled\n");
            free_instruction(ins);
            return 1;
        }
    } else if (ins.cmd == DECOMPRESS) {
        wordsize = ins.wordsize;

        int flag;
        if (!ins.out) {
            flag = decompress(ins.archive, ".", ins.files, ins.files_count, ins.dirs, ins.dirs_count);
        } else {
            flag = decompress(ins.archive, ins.out, ins.files, ins.files_count, ins.dirs, ins.dirs_count);
        }

        if (flag) {
            fprintf(stderr, "Decompression canceled\n");
            free_instruction(ins);
            return 1;
        }
    } else if (ins.cmd == LIST) {
        if (ins.files_count == 0) {
            fprintf(stderr, "Archive not specified\n");
            free_instruction(ins);
            return 1;
        }

        int flag;
        if (ins.dirs_count == 0) {
            flag = show_files(ins.files[0], NULL);
        } else {
            flag = show_files(ins.files[0], ins.dirs[0]);
        }

        if (flag) {
            fprintf(stderr, "Showing files canceled\n");
            free_instruction(ins);
            return 1;
        }
    } else {
        fprintf(stderr, "Invalid command, check avaiable commands with:\n");
        fprintf(stderr, "  %s -help\n", argv[0]);
        free_instruction(ins);
        return 1;
    }

    free_instruction(ins);
    return 0;
}