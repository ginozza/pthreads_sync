#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 256

int *vector_to_sort;
int v_size_global;
int *sorted_vector_output;
int min_val_global;
int max_val_global;
int main_bucket_size_global;
int num_main_buckets;

pthread_mutex_t coordinator_mutex;
pthread_cond_t new_main_bucket_cond;
pthread_barrier_t main_bucket_barrier;

int current_main_bucket_idx = -1;
volatile int shutdown_threads = 0;
int num_worker_threads;

void error(const char *err) {
  perror(err);
  exit(EXIT_FAILURE);
}

int *read_single_vector_from_file(FILE *file, int *size) {
  if (fscanf(file, "%d", size) != 1)
    return NULL;
  int *vec = malloc(*size * sizeof(int));
  if (!vec)
    error("malloc vector reading file");
  for (int i = 0; i < *size; ++i) {
    if (fscanf(file, "%d", &vec[i]) != 1) {
      free(vec);
      return NULL;
    }
  }
  return vec;
}

int find_max(int *vec, int size) {
  if (size <= 0)
    return 0;
  int max = vec[0];
  for (int i = 1; i < size; ++i)
    if (vec[i] > max)
      max = vec[i];
  return max;
}

int find_min(int *vec, int size) {
  if (size <= 0)
    return 0;
  int min = vec[0];
  for (size_t i = 1; i < size; ++i)
    if (vec[i] < min)
      min = vec[i];
  return min;
}

void sort_casillero(int *casillero, int size) {
  for (int i = 1; i < size; ++i) {
    int key = casillero[i];
    int j = i - 1;
    while (j >= 0 && casillero[j] > key) {
      casillero[j + 1] = casillero[j];
      j--;
    }
    casillero[j + 1] = key;
  }
}

void *worker_thread_func(void *arg) {
  long thread_id = (long)arg;
  int last_seen_bucket = -1;
  int local_count = 0;
  while (1) {
    pthread_mutex_lock(&coordinator_mutex);
    while (current_main_bucket_idx == last_seen_bucket && !shutdown_threads)
      pthread_cond_wait(&new_main_bucket_cond, &coordinator_mutex);
    if (shutdown_threads) {
      pthread_mutex_unlock(&coordinator_mutex);
      pthread_exit(NULL);
    }
    last_seen_bucket = current_main_bucket_idx;
    pthread_mutex_unlock(&coordinator_mutex);

    int my_main_bucket_idx = last_seen_bucket;
    int main_bucket_range_min =
        min_val_global + my_main_bucket_idx * main_bucket_size_global;
    int main_bucket_range_max =
        (my_main_bucket_idx == num_main_buckets - 1)
            ? max_val_global + 1
            : main_bucket_range_min + main_bucket_size_global;

    int casillero_range_size = main_bucket_size_global / num_worker_threads;
    if (thread_id == num_worker_threads - 1)
      casillero_range_size =
          main_bucket_size_global -
          thread_id * (main_bucket_size_global / num_worker_threads);

    int casillero_min =
        main_bucket_range_min +
        thread_id * (main_bucket_size_global / num_worker_threads);
    int casillero_max = casillero_min + casillero_range_size;

    int should_process_casillero = 1;
    if (my_main_bucket_idx == num_main_buckets - 1) {
      if (casillero_max > max_val_global + 1)
        casillero_max = max_val_global + 1;
      if (casillero_min >= max_val_global + 1)
        should_process_casillero = 0;
    }

    if (should_process_casillero) {
      int *temp_casillero = malloc(v_size_global * sizeof(int));
      if (!temp_casillero)
        error("malloc temp_casillero in worker");

      local_count = 0;
      for (int i = 0; i < v_size_global; ++i)
        if (vector_to_sort[i] >= casillero_min &&
            vector_to_sort[i] < casillero_max)
          temp_casillero[local_count++] = vector_to_sort[i];

      sort_casillero(temp_casillero, local_count);

      int global_offset = 0;
      for (int i = 0; i < my_main_bucket_idx; ++i) {
        int prev_min = min_val_global + i * main_bucket_size_global;
        int prev_max = (i == num_main_buckets - 1)
                           ? max_val_global + 1
                           : prev_min + main_bucket_size_global;
        for (int j = 0; j < v_size_global; ++j)
          if (vector_to_sort[j] >= prev_min && vector_to_sort[j] < prev_max)
            global_offset++;
      }

      for (long i = 0; i < thread_id; ++i) {
        int prev_min = main_bucket_range_min +
                       i * (main_bucket_size_global / num_worker_threads);
        int prev_max =
            prev_min + (main_bucket_size_global / num_worker_threads);
        if (i == num_worker_threads - 1)
          prev_max = main_bucket_range_min + main_bucket_size_global;
        if (prev_max > main_bucket_range_max)
          prev_max = main_bucket_range_max;
        if (prev_min < main_bucket_range_max)
          for (int j = 0; j < v_size_global; ++j)
            if (vector_to_sort[j] >= prev_min && vector_to_sort[j] < prev_max)
              global_offset++;
      }

      for (int i = 0; i < local_count; ++i)
        sorted_vector_output[global_offset + i] = temp_casillero[i];

      free(temp_casillero);
    }

    pthread_barrier_wait(&main_bucket_barrier);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 3)
    error("usage: <input_file> <main_bucket_size>");

  FILE *file = fopen(argv[1], "r");
  if (!file)
    error("error opening file");

  main_bucket_size_global = atoi(argv[2]);
  if (main_bucket_size_global <= 0)
    error("Main bucket size must be positive");

  vector_to_sort = read_single_vector_from_file(file, &v_size_global);
  if (!vector_to_sort)
    error("Error reading vector from file");
  fclose(file);

  min_val_global = find_min(vector_to_sort, v_size_global);
  max_val_global = find_max(vector_to_sort, v_size_global);

  if (v_size_global == 0)
    num_main_buckets = 0;
  else if (max_val_global == min_val_global)
    num_main_buckets = 1;
  else
    num_main_buckets =
        (max_val_global - min_val_global) / main_bucket_size_global + 1;

  sorted_vector_output = malloc(v_size_global * sizeof(int));
  if (!sorted_vector_output)
    error("malloc sorted_vector_output");

  pthread_mutex_init(&coordinator_mutex, NULL);
  pthread_cond_init(&new_main_bucket_cond, NULL);

  num_worker_threads = 8;
  pthread_t *worker_threads = malloc(num_worker_threads * sizeof(pthread_t));
  if (!worker_threads)
    error("malloc worker threads");

  pthread_barrier_init(&main_bucket_barrier, NULL, num_worker_threads + 1);

  for (long i = 0; i < num_worker_threads; ++i)
    pthread_create(&worker_threads[i], NULL, worker_thread_func, (void *)i);

  for (int i = 0; i < num_main_buckets; ++i) {
    pthread_mutex_lock(&coordinator_mutex);
    current_main_bucket_idx = i;
    pthread_cond_broadcast(&new_main_bucket_cond);
    pthread_mutex_unlock(&coordinator_mutex);
    pthread_barrier_wait(&main_bucket_barrier);
  }

  pthread_mutex_lock(&coordinator_mutex);
  shutdown_threads = 1;
  current_main_bucket_idx = -1;
  pthread_cond_broadcast(&new_main_bucket_cond);
  pthread_mutex_unlock(&coordinator_mutex);

  for (int i = 0; i < num_worker_threads; ++i)
    pthread_join(worker_threads[i], NULL);

  printf("Vector ordenado:\n");
  for (int i = 0; i < v_size_global; ++i) {
    printf("%d", sorted_vector_output[i]);
    if (i < v_size_global - 1)
      printf(", ");
  }
  printf("\n");

  pthread_barrier_destroy(&main_bucket_barrier);
  pthread_mutex_destroy(&coordinator_mutex);
  pthread_cond_destroy(&new_main_bucket_cond);

  free(worker_threads);
  free(vector_to_sort);
  free(sorted_vector_output);

  return 0;
}
