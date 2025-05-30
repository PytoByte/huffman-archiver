#include <stdio.h>
#include <stdlib.h>

#include "progbar.h"

static void pg_print(ProgressBar* progress_bar) {
    printf("\r");
    int percent = (100*progress_bar->value) / progress_bar->limit;
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    int fill = percent/10;

    printf("[");
    for (int j = 0; j < fill; j++) {
        printf("=");
    }
    for (int j = 0; j < 10 - fill; j++) {
        printf("-");
    }
    printf("] %3d%%", percent);
    fflush(stdout);
}

void pg_update(ProgressBar* progress_bar, long long delta) {
    progress_bar->value += delta;
    pg_print(progress_bar);
}

void pg_set(ProgressBar* progress_bar, long long value) {
    progress_bar->value = value;
    pg_print(progress_bar);
}

ProgressBar* pg_init(long long limit, long long start_value) {
    ProgressBar* progress_bar = (ProgressBar*)malloc(sizeof(ProgressBar));
    progress_bar->limit = limit;
    progress_bar->value = start_value;
    pg_print(progress_bar);
    return progress_bar;
}

void pg_end(ProgressBar* progress_bar) {
    progress_bar->value = progress_bar->limit;
    pg_print(progress_bar);
    free(progress_bar);
    printf("\n");
}