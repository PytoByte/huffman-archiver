// TODO ТРЕБОВАНИЕ К КОДЕРУ
// ШИФРОВАНИЕ ПО 4КБ (ну или любой другой буфер)

#define BUFFER_SIZE 4096

typedef struct {
    int count;
    int current;
    char* name;
    unsigned long long size;
    unsigned int treesize;
    unsigned long long filestart;
} FileFrame;

typedef struct {
    unsigned char* code;
    int size;
} Code;

typedef struct {
    Code* codes;
    int size;
} Codes;

void compress(int files_count, char** filenames, char* archivename);

void decompress(char* dir, char* archivename);