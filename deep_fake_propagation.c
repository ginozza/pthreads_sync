#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N_NEIGHBORS 8
#define N_THREADS 2

int **matrix;
int **temp_matrix;
int **next_state_temp;

int matrix_rows_size;
int matrix_cols_size;
int shifts;

pthread_barrier_t barrier;

void error(const char *err) {
  perror(err);
  exit(EXIT_FAILURE);
}

int **read_file(FILE *file, int *rows_size, int *cols_size) {
  fscanf(file, "%d", rows_size);
  fscanf(file, "%d", cols_size);

  int **matrix_alloc = malloc(*rows_size * sizeof(int *));
  if (!matrix_alloc)
    error("error malloc matrix read_file");

  for (int i = 0; i < *rows_size; ++i) {
    matrix_alloc[i] = malloc(*cols_size * sizeof(int));
    if (!matrix_alloc[i])
      error("malloc matrix columns read_file");
  }

  for (int i = 0; i < *rows_size; ++i)
    for (int j = 0; j < *cols_size; ++j)
      fscanf(file, "%d", &matrix_alloc[i][j]);

  fclose(file);
  return matrix_alloc;
}

int value_0_to_1(int **input_matrix, int x, int y, int rows, int cols) {
  int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
  int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

  int counter0_1 = 0;
  for (int i = 0; i < N_NEIGHBORS; ++i) {
    int nx = x + dx[i];
    int ny = y + dy[i];
    if (nx >= 0 && ny >= 0 && nx < rows && ny < cols &&
        input_matrix[nx][ny] > 0)
      counter0_1++;
  }
  if (input_matrix[x][y] == 0 && counter0_1 >= 2)
    return 1;
  return input_matrix[x][y];
}

int value_1_to_2(int **input_matrix, int x, int y, int rows, int cols) {
  if (input_matrix[x][y] != 1)
    return input_matrix[x][y];

  int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
  int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};
  int counter1_2 = 0;

  for (int i = 0; i < N_NEIGHBORS; ++i) {
    int nx = x + dx[i];
    int ny = y + dy[i];
    if (nx >= 0 && ny >= 0 && nx < rows && ny < cols) {
      if (input_matrix[nx][ny] == 2)
        counter1_2++;
    }
  }

  double P = 0.15 + (counter1_2 * 0.05);
  double r = (double)rand() / RAND_MAX;
  return (P > r) ? 2 : 1;
}

void *worker_thread(void *arg) {
  int thread_id = (int)(intptr_t)arg;
  for (int t = 0; t < shifts; ++t) {
    pthread_barrier_wait(&barrier);

    if (thread_id == 0) {
      for (int x = 0; x < matrix_rows_size; ++x) {
        for (int y = 0; y < matrix_cols_size; ++y) {
          next_state_temp[x][y] = value_0_to_1(
              temp_matrix, x, y, matrix_rows_size, matrix_cols_size);
        }
      }
    }

    pthread_barrier_wait(&barrier);

    if (thread_id == 1) {
      for (int x = 0; x < matrix_rows_size; ++x) {
        for (int y = 0; y < matrix_cols_size; ++y) {
          int current_val_from_temp = temp_matrix[x][y];
          if (current_val_from_temp == 1) {
            matrix[x][y] = value_1_to_2(temp_matrix, x, y, matrix_rows_size,
                                        matrix_cols_size);
          } else if (current_val_from_temp == 0) {
            matrix[x][y] = next_state_temp[x][y];
          } else {
            matrix[x][y] = current_val_from_temp;
          }
        }
      }
    }

    pthread_barrier_wait(&barrier);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  srand(time(NULL));

  if (argc != 3)
    error("Usage: <file> <shifts>");

  char *filename = argv[1];
  FILE *file = fopen(filename, "r");
  if (!file)
    error("error opening file");

  shifts = atoi(argv[2]);

  temp_matrix = read_file(file, &matrix_rows_size, &matrix_cols_size);

  printf("Estado inicial\n");
  for (int x = 0; x < matrix_rows_size; ++x) {
    for (int y = 0; y < matrix_cols_size; ++y)
      printf("%3d", temp_matrix[x][y]);
    printf("\n");
  }
  printf("\n");

  matrix = malloc(matrix_rows_size * sizeof(int *));
  if (!matrix)
    error("error malloc matrix main");
  for (int i = 0; i < matrix_rows_size; ++i) {
    matrix[i] = malloc(matrix_cols_size * sizeof(int));
    if (!matrix[i])
      error("malloc matrix columns main");
  }

  next_state_temp = malloc(matrix_rows_size * sizeof(int *));
  if (!next_state_temp)
    error("error malloc next_state_temp");
  for (int i = 0; i < matrix_rows_size; ++i) {
    next_state_temp[i] = malloc(matrix_cols_size * sizeof(int));
    if (!next_state_temp[i])
      error("malloc next_state_temp columns");
  }

  pthread_t threads[N_THREADS];
  pthread_barrier_init(&barrier, NULL, N_THREADS + 1);

  for (int i = 0; i < N_THREADS; ++i) {
    pthread_create(&threads[i], NULL, worker_thread, (void *)(intptr_t)i);
  }

  for (int i = 1; i <= shifts; ++i) {
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    printf("Iteracion %d\n", i);
    for (int x = 0; x < matrix_rows_size; ++x) {
      for (int y = 0; y < matrix_cols_size; ++y)
        printf("%3d", matrix[x][y]);
      printf("\n");
    }
    printf("\n");

    for (int x = 0; x < matrix_rows_size; ++x)
      for (int y = 0; y < matrix_cols_size; ++y)
        temp_matrix[x][y] = matrix[x][y];
  }

  for (int i = 0; i < N_THREADS; ++i)
    pthread_join(threads[i], NULL);

  pthread_barrier_destroy(&barrier);

  for (int i = 0; i < matrix_rows_size; ++i) {
    free(temp_matrix[i]);
    free(matrix[i]);
    free(next_state_temp[i]);
  }
  free(temp_matrix);
  free(matrix);
  free(next_state_temp);

  return EXIT_SUCCESS;
}
