#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAX_LINE 1024
#define MAX_REQUESTS 250
#define NO_CLASIFICADO -1
#define BAJA 0
#define URGENTE 1
#define CRITICA 2
#define EN_VERIFICACION 3
#define N_THREADS 2

const char *palabras_criticas[] = {"servidor", "bloqueo", "caída"};
const int n_palabras = sizeof(palabras_criticas) / sizeof(palabras_criticas[0]);

typedef struct {
  char buffer[MAX_LINE];
  int status;
} Request;

Request requests[MAX_REQUESTS];
int total_requests = 0;

Request criticas[MAX_REQUESTS];
int total_criticas = 0;

Request urgentes[MAX_REQUESTS];
int total_urgentes = 0;

Request baja_prioridad[MAX_REQUESTS];
int total_baja = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_barrier_t barrier;
int listo = 0;

void error(const char *err) {
  perror(err);
  exit(EXIT_FAILURE);
}

void read_file(FILE *file) {
  int temp_v_size;
  fscanf(file, "%d\n", &temp_v_size);

  total_requests = temp_v_size;

  for (int i = 0; i < total_requests; ++i) {
    if (fgets(requests[i].buffer, MAX_LINE, file) == NULL) {
      break;
    }
    size_t len = strlen(requests[i].buffer);
    if (len > 0 && requests[i].buffer[len - 1] == '\n') {
      requests[i].buffer[len - 1] = '\0';
    }
    requests[i].status = NO_CLASIFICADO;
  }
  fclose(file);
}

void *thread_worker(void *arg) {
  int thread_id = (int)(intptr_t)arg;

  pthread_mutex_lock(&mutex);
  while (!listo) {
    pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);

  if (thread_id == 0) {
    for (int i = 0; i < total_requests; ++i) {
      pthread_mutex_lock(&mutex);
      if (requests[i].status == NO_CLASIFICADO) {
        if (strncmp(requests[i].buffer, "REQ:", 4) == 0 &&
            strchr(requests[i].buffer, ';') != NULL) {
          if (strstr(requests[i].buffer, "URGENTE")) {
            requests[i].status = EN_VERIFICACION;
          } else {
            baja_prioridad[total_baja++] = requests[i];
            requests[i].status = BAJA;
          }
        } else {
          baja_prioridad[total_baja++] = requests[i];
          requests[i].status = BAJA;
        }
      }
      pthread_mutex_unlock(&mutex);
    }
  }

  pthread_barrier_wait(&barrier);

  if (thread_id == 1) {
    for (int i = 0; i < total_requests; ++i) {
      pthread_mutex_lock(&mutex);
      if (requests[i].status == EN_VERIFICACION) {
        int es_critica = 0;
        for (int j = 0; j < n_palabras; ++j) {
          if (strstr(requests[i].buffer, palabras_criticas[j])) {
            es_critica = 1;
            break;
          }
        }
        if (es_critica) {
          criticas[total_criticas++] = requests[i];
          requests[i].status = CRITICA;
        } else {
          urgentes[total_urgentes++] = requests[i];
          requests[i].status = URGENTE;
        }
      }
      pthread_mutex_unlock(&mutex);
    }
  }

  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    error("Usage: <filename>");

  char *filename = argv[1];
  FILE *file = fopen(filename, "r");
  if (!file)
    error("error opening file main");

  pthread_t threads[N_THREADS];

  pthread_barrier_init(&barrier, NULL, N_THREADS );

  for (int i = 0; i < N_THREADS; ++i) {
    if (pthread_create(&threads[i], NULL, thread_worker, (void *)(intptr_t)i) !=
        0)
      error("thread creation");
  }

  read_file(file);

  listo = 1;
  pthread_cond_broadcast(&cond);

  for (int i = 0; i < N_THREADS; ++i)
    pthread_join(threads[i], NULL);

  printf("\n[CRÍTICAS]\n");
  if (total_criticas == 0) {
    printf("No hay solicitudes críticas.\n");
  } else {
    for (int i = 0; i < total_criticas; ++i)
      printf("%s\n", criticas[i].buffer);
  }

  printf("\n[URGENTES]\n");
  if (total_urgentes == 0) {
    printf("No hay solicitudes urgentes.\n");
  } else {
    for (int i = 0; i < total_urgentes; ++i)
      printf("%s\n", urgentes[i].buffer);
  }

  printf("\n[BAJA_PRIORIDAD]\n");
  if (total_baja == 0) {
    printf("No hay solicitudes de baja prioridad.\n");
  } else {
    for (int i = 0; i < total_baja; ++i)
      printf("%s\n", baja_prioridad[i].buffer);
  }

  pthread_barrier_destroy(&barrier);
  pthread_mutex_destroy(&mutex);

  return EXIT_SUCCESS;
}
