#include <stdio.h>
#include <stdlib.h>

#include "progbar.h"

static int current_percent = -1;
static unsigned long long limit = 1;
static unsigned long long value = 1;

static void pg_print() {
    int fill_count = 40;
    int max = 100;
    int percent = (max*value) / limit;
    if (current_percent == percent) {
        return;
    }
    current_percent = percent;

    printf("\r");
    if (percent < 0) {
        percent = 0;
    } else if (percent > max) {
        percent = max;
    }
    int fill = (fill_count * percent) / max;

    printf("[");
    for (int j = 0; j < fill; j++) {
        printf("=");
    }
    for (int j = 0; j < fill_count - fill; j++) {
        printf("-");
    }
    printf("] %3d%%", percent);
    //printf(" %lld/%lld", value, limit);
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
    current_percent = -1;
    printf("\n");
}