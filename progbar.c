#include <stdio.h>
#include <stdlib.h>

#include "progbar.h"

static unsigned long long limit;
static unsigned long long value;

static void pg_print() {
    int fill_count = 20;

    printf("\r");
    int percent = (100*value) / limit;
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    int fill = (fill_count * percent) / 100;

    printf("[");
    for (int j = 0; j < fill; j++) {
        printf("=");
    }
    for (int j = 0; j < fill_count - fill; j++) {
        printf("-");
    }
    printf("] %3d%%", percent);
    fflush(stdout);
}

void pg_update(long long delta) {
    value += delta;
    pg_print();
}

void pg_set(long long new_value) {
    value = new_value;
    pg_print();
}

void pg_init(long long lim, long long start_value) {
    limit = lim;
    value = start_value;
    pg_print();
}

void pg_end() {
    value = limit;
    pg_print();
    printf("\n");
}