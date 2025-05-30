typedef struct {
    long long limit;
    long long value;
} ProgressBar;

void pg_update(ProgressBar* progress_bar, long long delta);

void pg_set(ProgressBar* progress_bar, long long value);

ProgressBar* pg_init(long long limit, long long start_value);

void pg_end(ProgressBar* progress_bar);