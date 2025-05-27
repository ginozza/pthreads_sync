#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>

#define MAX_POPULATION_CONST 250

typedef struct {
  double feature1;
  double feature2;
  double fitness;
} Individual;

typedef struct {
  Individual* individual;
  int start_index;
  int end_index;
  int thread_id;
  pthread_barrier_t* barrier;
  int* current_generation;
  int total_generations;
  pthread_mutex_t* individuals_mutex;
} ThreadArgs;

Individual* g_individuals;
int g_n_individuals;
pthread_mutex_t individuals_mutex;
pthread_barrier_t generation_barrier;
int g_current_generation = 0;
int g_total_generations;

void error(const char* err) {
  perror(err);
  exit(EXIT_FAILURE);
}

void println(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  putchar('\n');
}

double fitness_calc(double feature1, double feature2) {
  return (feature1 * feature2) / 2.0;
}

int compare_individuals(const void *a, const void* b) {
  Individual* individualA = (Individual*)a;
  Individual* individualB = (Individual*)b;
  if (individualA->fitness < individualB->fitness) {
    return 1;
  } else if (individualA->fitness > individualB->fitness) {
    return -1;
  } else {
    return 0;
  }
}

void* worker_thread(void* arg) {
  ThreadArgs* args = (ThreadArgs*)arg;
  Individual* local_individual = args->individual;
  int start = args->start_index;
  int end = args->end_index;
  pthread_barrier_t* barrier = args->barrier;
  int* current_generation = args->current_generation;
  int total_generations = args->total_generations;
  pthread_mutex_t* individuals_mutex = args->individuals_mutex;

  while (1) {
    pthread_barrier_wait(barrier);

    if (*current_generation > total_generations) {
      break;
    }

    for (int i = start; i < end; ++i) {
      local_individual[i].fitness = fitness_calc(
        local_individual[i].feature1, local_individual[i].feature2
      );
      local_individual[i].feature1 = (double)rand() / RAND_MAX * MAX_POPULATION_CONST;
      local_individual[i].feature2 = (double)rand() / RAND_MAX * MAX_POPULATION_CONST;
    }

    pthread_barrier_wait(barrier);

    if (args->thread_id == 0) {
      pthread_mutex_lock(individuals_mutex);
      qsort(g_individuals, g_n_individuals, sizeof(Individual), compare_individuals);
      pthread_mutex_unlock(individuals_mutex);

      println("Generacion %d", *current_generation);
      for (int i = 0; i < g_n_individuals && i < 10; ++i)
        println("  Individuo %d: F1=%.2f, F2=%.2f, Fitness=%.2f",
                i + 1,
                g_individuals[i].feature1,
                g_individuals[i].feature2,
                g_individuals[i].fitness);
    }
    
    pthread_barrier_wait(barrier);
  }
  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char* argv[]) {
  if(argc != 4) error("Usage: <n_individuals> <n_threads> <n_generations>");

  g_n_individuals = atoi(argv[1]);
  int n_threads = atoi(argv[2]);
  g_total_generations = atoi(argv[3]);

  srand(time(NULL));

  g_individuals = (Individual*)malloc(g_n_individuals * sizeof(Individual));
  if (!g_individuals) error("error malloc g_individuals");

  pthread_mutex_init(&individuals_mutex, NULL);
  pthread_barrier_init(&generation_barrier, NULL, n_threads + 1);

  for (int i = 0; i < g_n_individuals; ++i) {
    g_individuals[i].feature1 = (double)rand() / RAND_MAX * MAX_POPULATION_CONST;
    g_individuals[i].feature2 = (double)rand() / RAND_MAX * MAX_POPULATION_CONST;
    g_individuals[i].fitness = 0.0;
  }

  pthread_t threads[n_threads];
  ThreadArgs thread_args[n_threads];

  int individuals_per_thread = g_n_individuals / n_threads;
  int remainder = g_n_individuals % n_threads;
  int current_index = 0;

  for (int i = 0; i < n_threads; ++i) {
    thread_args[i].individual = g_individuals;
    thread_args[i].start_index = current_index;
    thread_args[i].end_index = current_index + individuals_per_thread + (i < remainder ? 1 : 0);
    thread_args[i].thread_id = i;
    thread_args[i].barrier = &generation_barrier;
    thread_args[i].current_generation = &g_current_generation;
    thread_args[i].total_generations = g_total_generations;
    thread_args[i].individuals_mutex = &individuals_mutex;

    if (pthread_create(&threads[i], NULL, worker_thread, (void*)&thread_args[i]) != 0)
      error("error pthread_create");

    current_index = thread_args[i].end_index;
  }

  for (int k = 1; k <= g_total_generations; ++k) {
    g_current_generation = k;
    pthread_barrier_wait(&generation_barrier);
    pthread_barrier_wait(&generation_barrier);
    pthread_barrier_wait(&generation_barrier);
  }

  g_current_generation = g_total_generations + 1;
  pthread_barrier_wait(&generation_barrier);

  for (int i = 0; i < n_threads; ++i)
    pthread_join(threads[i], NULL);

  pthread_mutex_destroy(&individuals_mutex);
  pthread_barrier_destroy(&generation_barrier);

  free(g_individuals);

  return EXIT_SUCCESS;
}
