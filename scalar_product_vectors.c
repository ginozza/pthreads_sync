#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>

int* vectorA;
int* vectorB;
int* product_vector;
int v_size;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int start;
  int end;
} ThreadArgs;

void error(const char* err) {
  perror(err);
  exit(EXIT_FAILURE);
}

void println(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  putchar('\n');
}

void read_file(FILE *file, int** vectorA, int** vectorB, int* v_size) {
  fscanf(file, "%d", v_size);

  *vectorA = (int*)malloc(*v_size * sizeof(int));
  if (!(*vectorA)) error("error malloc read_file vectorA");

  for (int i = 0; i < *v_size; ++i)
    fscanf(file, "%d", &(*vectorA)[i]);

  *vectorB = (int*)malloc(*v_size * sizeof(int));
  if (!(*vectorB)) error ("error malloc read_file vectorB");

  for (int i = 0; i < *v_size; ++i)
    fscanf(file, "%d", &(*vectorB)[i]);

  fclose(file);
}

void* product(void* arg) {
  ThreadArgs* args = (ThreadArgs*)arg;
  int start = args->start;
  int end = args->end;

  for (int i = start; i < end; ++i) {

    pthread_mutex_lock(&mutex);
    product_vector[i] = vectorA[i] * vectorB[i];
    pthread_mutex_unlock(&mutex);

    println("VectorA[%d] * VectorB[%d] = %d", 
            vectorA[i], vectorB[i], vectorA[i] * vectorB[i]);
  }

  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char* argv[]) {
  if(argc != 3) error("Usage: <filename> <n_threads>");

  const char* filename = argv[1];
  FILE* file = fopen(filename, "r");
  if (!file) error("error opening file");

  read_file(file, &vectorA, &vectorB, &v_size);

  product_vector = (int*)malloc(v_size * sizeof(int));

  int n_threads = atoi(argv[2]);

  pthread_t threads[n_threads];
  ThreadArgs thread_args[n_threads];

  int elements_per_thread = v_size / n_threads;
  int remainder = v_size % n_threads;
  int current_index = 0;

  for (int i = 0; i < n_threads; ++i) {
    thread_args[i].start = current_index;
    thread_args[i].end = current_index + elements_per_thread + (i < remainder ? 1 : 0);

    if (pthread_create(&threads[i], NULL, product, (void*)&thread_args[i]) != 0)
      error("error pthread_create");

    current_index = thread_args[i].end;
  }

  for (int i = 0; i < n_threads; ++i)
    pthread_join(threads[i], NULL);

  println("");
  for (int i = 0; i < v_size; ++i)
    printf("%d ", product_vector[i]);
  println("");

  free(vectorA);
  free(vectorB);
  free(product_vector);

  return EXIT_SUCCESS;
}
