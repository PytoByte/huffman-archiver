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
    OPTION_DECLINEWARNING = 3,
    OPTION_ACCEPTWARNING = 4,
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
    enum WarningAction warning_action;
} Instruction;

typedef struct Manual {
    const int aliases_count;
    const char** aliases;
    const char* description;
    const char* usage;
} Manual;

Manual commands_manual[] = {
    {2, (const char*[]){"-help", "-h"}, "Show help information", "-help"},
    {2, (const char*[]){"-compress", "-c"}, "Compress files", "-compress [files|dirs] [-output <file>] [-word <number>] [-dw|aw]"},
    {2, (const char*[]){"-decompress", "-d"}, "Decompress files", "-decompress <archive> [-output <dir>] [files] [-dir <path>]"},
    {2, (const char*[]){"-list", "-ls"}, "Show list of files in archive. Use -dir to select dir in archive", "-list <archive> [-dir <path>]"},
    {0, NULL, NULL, NULL}
};

Manual options_manual[] = {
    {2, (const char*[]){"-output", "-o"}, "Specify path for command", "-output <file|dir>"},
    {2, (const char*[]){"-word", "-w"}, "Specify word size in bytes (from 1 to 2)", "-word <number>"},
    {1, (const char*[]){"-dir"}, "Specify directory inside archive", "-dir <path>"},
    {2, (const char*[]){"-declinewarning", "-dw"}, "Decline all warnings about small files", "-declinewarning"},
    {2, (const char*[]){"-acceptwarning", "-aw"}, "Accept all warnings about small files", "-acceptwarning"},
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
    Instruction ins = {INVALID_COMMAND, NULL, NULL, NULL, 0, NULL, 0, 1, WARN_ACT_ASK};

    ins.files = (char**)malloc(argc * sizeof(char*));
    if (!ins.files) {
        ins.cmd = PARSER_ERROR;
        fprintf(stderr, "Memory allocation failed\n");
        return ins;
    }

    ins.dirs = (char**)malloc((argc/2) * sizeof(char*));
    if (!ins.files) {
        ins.cmd = PARSER_ERROR;
        fprintf(stderr, "Memory allocation failed\n");
        free(ins.files);
        return ins;
    }

    int check = 0;
    for (int i = 0; i < argc; i++) {
        check = check_flag(argv[i], commands_manual[HELP].aliases, commands_manual[HELP].aliases_count);
        if (check == 1) {
            ins.cmd = HELP;
            return ins;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], commands_manual[COMPRESS].aliases, commands_manual[COMPRESS].aliases_count);
        if (check == 1) {
            if (ins.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
            ins.cmd = COMPRESS;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], commands_manual[DECOMPRESS].aliases, commands_manual[DECOMPRESS].aliases_count);
        if (check == 1) {
            if (ins.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }

            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }

            ins.archive = argv[i+1];
            i += 1;

            ins.cmd = DECOMPRESS;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], commands_manual[LIST].aliases, commands_manual[LIST].aliases_count);
        if (check == 1) {
            if (ins.cmd != INVALID_COMMAND) {
                fprintf(stderr, "Only one command can be specified\n");
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
            ins.cmd = LIST;
            ins.archive = argv[i+1];
            i += 1;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], options_manual[OPTION_OUTPUT].aliases, options_manual[OPTION_OUTPUT].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
            ins.out = argv[i+1];
            i += 1;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], options_manual[OPTION_WORDSIZE].aliases, options_manual[OPTION_WORDSIZE].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }

            int parse_wordsize = atoi(argv[i+1]);
            if (parse_wordsize < 1 || parse_wordsize>2) {
                fprintf(stderr, "Option \"%s\" can take only one of the following values: from 1 to 2\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }

            ins.wordsize = parse_wordsize;
            i += 1;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], options_manual[OPTION_DIR].aliases, options_manual[OPTION_DIR].aliases_count);
        if (check == 1) {
            if (argc <= i+1) {
                fprintf(stderr, "Option \"%s\" requires 1 argument\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }

            for (int j = 0; j < ins.dirs_count; j++) {
                if (strcmp(ins.dirs[j], argv[i+1]) == 0) {
                    fprintf(stderr, "Directory \"%s\" is specified twice\n", argv[i+1]);
                    ins.cmd = PARSER_ERROR;
                    free_instruction(ins);
                    return ins;
                }
            }

            ins.dirs[ins.dirs_count++] = argv[i+1];
            i += 1;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], options_manual[OPTION_DECLINEWARNING].aliases, options_manual[OPTION_DECLINEWARNING].aliases_count);
        if (check == 1) {
            if (ins.warning_action != WARN_ACT_ASK) {
                fprintf(stderr, "Only one warning option can be specified\n");
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
            ins.warning_action = WARN_ACT_DECLINE;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        check = check_flag(argv[i], options_manual[OPTION_ACCEPTWARNING].aliases, options_manual[OPTION_ACCEPTWARNING].aliases_count);
        if (check == 1) {
            if (ins.warning_action != WARN_ACT_ASK) {
                fprintf(stderr, "Only one warning option can be specified\n");
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
            ins.warning_action = WARN_ACT_ACCEPT;
            continue;
        } else if (check == -1) {
            ins.cmd = PARSER_ERROR;
            free_instruction(ins);
            return ins;
        }

        for (int i = 0; i < ins.files_count; i++) {
            if (strcmp(ins.files[i], argv[i]) == 0) {
                fprintf(stderr, "File \"%s\" is specified twice\n", argv[i]);
                ins.cmd = PARSER_ERROR;
                free_instruction(ins);
                return ins;
            }
        }
        ins.files[ins.files_count++] = argv[i];
    }

    return ins;
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
        compress_warn_act = ins.warning_action;

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
        if (ins.archive == NULL) {
            fprintf(stderr, "Archive not specified\n");
            free_instruction(ins);
            return 1;
        }

        int flag;
        if (ins.dirs_count == 0) {
            flag = show_files(ins.archive, NULL);
        } else {
            flag = show_files(ins.archive, ins.dirs[0]);
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