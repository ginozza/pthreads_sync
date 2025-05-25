#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define MAX_LINE 256

int* vector;
int v_size;
int* sorted_vector;
int min_val;
int max_val;
int bucket_size;

void error(const char* err) {
  perror(err);
  exit(EXIT_SUCCESS);
}

int* read_vector(FILE* file) {
  fscanf(file, "%d", &v_size);

  int* vector = malloc(v_size * sizeof(int));
  if (!vector) error("malloc vector reading file");

  for (int i = 0; i < v_size; ++i)
    fscanf(file, "%d", &vector[i]);

  fclose(file);
  return vector;
}

int find_max(int* vector, int v_size) {
  int max = vector[0];
  for (int i = 1; i < v_size; ++i)
    if (vector[i] > max)
      max = vector[i];
  return max;
}

int find_min(int* vector, int v_size) {
  int min = vector[0];
  for (size_t i = 1; i < v_size; ++i)
    if (vector[i] < min)
      min = vector[i];
  return min;
}

void sort_bucket(int* bucket, int size) {
  for (int i = 1; i < size; ++i) {
    int key = bucket[i];
    int j = i - 1;

    while (j >= 0 && bucket[j] > key) {
      bucket[j + 1] = bucket[j];
      j--;
    }
    bucket[j + 1] = key;
  }
}

void* bucket_sort(void *arg) {
  int thread_id = (int)(intptr_t)arg;

  int range_min = min_val + thread_id * bucket_size;
  int range_max = (thread_id == (max_val - min_val) / bucket_size) ? max_val + 1 : range_min + bucket_size;

  int* bucket = malloc(v_size * sizeof(int));
  if (!bucket) error("malloc bucket");

  int count = 0;
  for (int i = 0; i < v_size; ++i) {
    if (vector[i] >= range_min && vector[i] < range_max) {
      bucket[count++] = vector[i];
    }
  }

  sort_bucket(bucket, count);

  int offset = 0;
  for (int i = 0; i < thread_id; ++i) {
    int prev_min = min_val + i * bucket_size;
    int prev_max = prev_min + bucket_size;
    for (int j = 0; j < v_size; ++j) {
      if (vector[j] >= prev_min && vector[j] < prev_max) {
        offset++;
      }
    }
  }

  for (int i = 0; i < count; ++i)
    sorted_vector[offset + i] = bucket[i];

  free(bucket);
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc != 3) error("usage: <input_file> <bucket_size>");

  char* filename = argv[1];
  FILE *file = fopen(filename, "r");
  if (!file) error("error opening file");

  vector = read_vector(file);

  max_val = find_max(vector, v_size);
  min_val = find_min(vector, v_size);

  bucket_size = atoi(argv[2]);
  
  int n_threads = (max_val - min_val) / bucket_size + 1;

  sorted_vector = malloc(v_size * sizeof(int));
  if (!sorted_vector) error("malloc sorted vector");

  pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
  if (!threads) error("malloc threads");

  for (int i = 0; i < n_threads; ++i)
    pthread_create(&threads[i], NULL, bucket_sort, (void *)(intptr_t)i);

  for (int i = 0; i < n_threads; ++i)
    pthread_join(threads[i], NULL);

  printf("Vector ordenado:\n");
  for (int i = 0; i < v_size; ++i)
    printf("%d, ", sorted_vector[i]);

  free(vector);
  free(sorted_vector);
  free(threads);

  return EXIT_SUCCESS;
}
