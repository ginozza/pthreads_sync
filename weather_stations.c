/* The program processes multiple CSV-formatted text files provided via command-line arguments. Each file corresponds to a temperature zone and contains lines formatted as date,temperature, where only the temperature (second column) is parsed and averaged—date values in the first column are ignored. A separate thread is created for each file to compute the average temperature concurrently. After all threads finish, the program prints the average temperature for each zone and identifies the warmest and coldest zones. */ 

#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_FILE_NAME 256

typedef struct {
  char filename[MAX_FILE_NAME];
  char zone_name[40];
  double mean;
  int thread_index;
} StationData;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

double max_mean = -1.0;
double min_mean = __DBL_MAX__;
char max_zone[40];
char min_zone[40];
int end_threads = 0;

void error(const char *err) {
  perror(err);
  exit(EXIT_FAILURE);
}

void *check_zone(void *arg) {
  StationData* data = (StationData*)arg;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  int n_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  CPU_SET(data->thread_index % n_cpus, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

  FILE* file = fopen(data->filename, "r");
  if (!file) error("error opening file");

  char buffer_line[MAX_LINE];
  fgets(buffer_line, MAX_LINE, file);

  double add = 0.0;
  int counter = 0;
  while (fgets(buffer_line, MAX_LINE, file)) {
    char *token = strtok(buffer_line, ",");
    token = strtok(NULL, ",");
    if (token) {
      add += atof(token);
      counter++;
    }
  }
  fclose(file);

  data->mean = add / counter;

  pthread_mutex_lock(&mutex);
  if (data->mean > max_mean) {
    max_mean = data->mean;
    strcpy(max_zone, data->zone_name);
  }
  if (data->mean < min_mean) {
    min_mean = data->mean;
    strcpy(min_zone, data->zone_name);
  }
  end_threads++;
  pthread_cond_signal(&cond); // notificar al hijo principal fin de condicion de espera
  pthread_mutex_unlock(&mutex);

  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  if (argc < 2) error("uso: archivo1.txt archivo2.txt...");

  int n_files = argc - 1;
  pthread_t* threads = malloc(n_files * sizeof(pthread_t));
  StationData* data = malloc(n_files * sizeof(StationData));

  for (int i = 0; i < n_files; ++i) {
    strncpy(data[i].filename, argv[i + 1], sizeof(data[i].filename));
    snprintf(data[i].zone_name, sizeof(data[i].zone_name), "Zona %d", i + 1);
    data[i].mean = 0.0;

    if (pthread_create(&threads[i], NULL, check_zone, &data[i]) != 0)
      error("error creating thread");
  }

  pthread_mutex_lock(&mutex);
  while (end_threads < n_files) {
    pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);

  for (int i = 0; i < n_files; ++i)
    pthread_join(threads[i], NULL);

  printf("Results:\n");
  for (int i = 0; i < n_files; ++i)
    printf("%s: Promedio = %.2f°C\n", data[i].zone_name, data[i].mean);
  printf("Zona mas calida: %s (%.2f°C)\n", max_zone, max_mean);
  printf("Zona menos calida %s (%.2f°C)\n", min_zone, min_mean);

  free(threads);
  free(data);
  return EXIT_SUCCESS;
}
