#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

int **g_current_matrix;
int **g_next_matrix;
int g_rows, g_cols;
int g_iterations;

pthread_barrier_t barrier;

typedef struct {
    int thread_id;
    int start_row;
    int end_row;
} ThreadArgs;

void error(const char* err) {
    perror(err);
    exit(EXIT_FAILURE);
}

int value(int **mat, int x, int y) {
    int top = mat[x - 1][y];
    int left = mat[x][y - 1];
    int right = mat[x][y + 1];
    int bottom = mat[x + 1][y];
    return (top + left + right + bottom) / 4;
}

int** allocate_2d_matrix(int rows, int cols) {
    int** matrix = (int**)malloc(rows * sizeof(int*));
    if (!matrix) error("allocate_2d_matrix: failed to allocate row pointers");

    matrix[0] = (int*)malloc(rows * cols * sizeof(int));
    if (!matrix[0]) {
        free(matrix);
        error("allocate_2d_matrix: failed to allocate matrix elements");
    }

    for (int i = 1; i < rows; ++i) {
        matrix[i] = matrix[i - 1] + cols;
    }
    return matrix;
}

void free_2d_matrix(int** matrix) {
    if (matrix) {
        free(matrix[0]);
        free(matrix);
    }
}

int **read_file(const char* filename, int* rows, int* cols) {
    FILE *file = fopen(filename, "r");
    if (!file) error("read_file: cannot open file");

    fscanf(file, "%d", rows);
    fscanf(file, "%d", cols);

    int **matrix = allocate_2d_matrix(*rows, *cols);

    for (int i = 0; i < *rows; ++i)
        for (int j = 0; j < *cols; ++j)
            fscanf(file, "%d", &matrix[i][j]);

    fclose(file);
    return matrix;
}

void copy_matrix(int **src, int **dest, int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            dest[i][j] = src[i][j];
        }
    }
}

void* worker_thread(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int start_row = args->start_row;
    int end_row = args->end_row;

    for (int t = 0; t < g_iterations; ++t) {
        for (int i = start_row; i < end_row; ++i) {
            for (int j = 0; j < g_cols; ++j) {
                if (i == 0 || i == g_rows - 1 || j == 0 || j == g_cols - 1) {
                    g_next_matrix[i][j] = g_current_matrix[i][j];
                    continue;
                }
                g_next_matrix[i][j] = value(g_current_matrix, i, j);
            }
        }

        pthread_barrier_wait(&barrier);

        for (int i = start_row; i < end_row; ++i) {
            for (int j = 0; j < g_cols; ++j) {
                g_current_matrix[i][j] = g_next_matrix[i][j];
            }
        }

        pthread_barrier_wait(&barrier);
    }
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <filename> <n_threads> <iterations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* filename = argv[1];
    int n_threads = atoi(argv[2]);
    g_iterations = atoi(argv[3]);

    if (n_threads <= 0 || g_iterations <= 0) {
        fprintf(stderr, "Error: Number of threads and iterations must be greater than 0.\n");
        exit(EXIT_FAILURE);
    }

    int local_rows, local_cols;
    int **temp_matrix = read_file(filename, &local_rows, &local_cols);

    g_rows = local_rows;
    g_cols = local_cols;

    g_current_matrix = allocate_2d_matrix(g_rows, g_cols);
    g_next_matrix = allocate_2d_matrix(g_rows, g_cols);

    copy_matrix(temp_matrix, g_current_matrix, g_rows, g_cols);
    free_2d_matrix(temp_matrix);

    if (pthread_barrier_init(&barrier, NULL, n_threads + 1) != 0) {
        error("pthread_barrier_init failed");
    }

    pthread_t threads[n_threads];
    ThreadArgs thread_args[n_threads];

    int inner_rows = g_rows - 2;
    if (inner_rows < 0) inner_rows = 0;

    int rows_per_thread = inner_rows / n_threads;
    int remainder = inner_rows % n_threads;

    int current_row_for_threads = 1;

    for (int i = 0; i < n_threads; ++i) {
        thread_args[i].thread_id = i;
        thread_args[i].start_row = current_row_for_threads;
        thread_args[i].end_row = current_row_for_threads + rows_per_thread + (i < remainder ? 1 : 0);

        if (thread_args[i].end_row > g_rows - 1) {
            thread_args[i].end_row = g_rows - 1;
        }

        if (pthread_create(&threads[i], NULL, worker_thread, (void*)&thread_args[i]) != 0) {
            error("pthread_create failed");
        }
        current_row_for_threads = thread_args[i].end_row;
    }

    for (int t = 0; t < g_iterations; ++t) {
        pthread_barrier_wait(&barrier);

        printf("Paso %d:\n", t + 1);
        for (int i = 0; i < g_rows; ++i) {
            for (int j = 0; j < g_cols; ++j) {
                printf("%3d", g_current_matrix[i][j]);
            }
            printf("\n");
        }
        printf("\n");

        pthread_barrier_wait(&barrier);
    }

    for (int i = 0; i < n_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);

    free_2d_matrix(g_current_matrix);
    free_2d_matrix(g_next_matrix);

    return EXIT_SUCCESS;
}
