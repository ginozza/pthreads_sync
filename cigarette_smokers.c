#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_smoker_tobacco = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_smoker_paper = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_smoker_matches = PTHREAD_COND_INITIALIZER;

int has_tobacco = 0;
int has_paper = 0;
int has_matches = 0;

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

void* smoker_tobacco(void* __) {
  while (1) {
    pthread_mutex_lock(&mutex);

    println("Fumador [Tabaco]: Esperando papel y fósforos");
    pthread_cond_wait(&cond_smoker_tobacco, &mutex);

    has_paper = 0;
    has_matches = 0;

    println("Fumador [Tabaco]: Ha tomado papel y fósforos, haciendo cigarrillo");

    pthread_mutex_unlock(&mutex);

    sleep(rand() % 3 + 1);
    println("Fumador [Tabaco]: Ha dejado de fumar");
  }
  return NULL;
}

void* smoker_paper(void* __) {
  while (1) {
    pthread_mutex_lock(&mutex);

    println("Fumador [Papel]: Esperando tabaco y fósforos");
    pthread_cond_wait(&cond_smoker_paper, &mutex);

    has_tobacco = 0;
    has_matches = 0;

    println("Fumador [Papel]: Ha tomado tabaco y fósforos, haciendo cigarrillo");

    pthread_mutex_unlock(&mutex);

    sleep(rand() % 3 + 1);
    println("Fumador [Papel]: Ha dejado de fumar");
  }
  return NULL;
}

void* smoker_matches(void* __) {
  while (1) {
    pthread_mutex_lock(&mutex);

    println("Fumador [Fósforos]: Esperando tabaco y papel");

    has_tobacco = 0;
    has_paper = 0;

    println("Fumador [Fósforos]: Ha tomando tabaco y papel, haciendo cigarrillo");

    pthread_mutex_unlock(&mutex);

    sleep(rand() % 3 + 1);
    println("Fumador [Fósforos]: Ha dejado de fumar");
  }
  return NULL;
}

void* smokers_agent(void* __) {
  srand(time(NULL));
  while(1) {
    pthread_mutex_lock(&mutex);

    int choice = rand() % 3;
    switch (choice) {
      case 0:
        has_paper = 1;
        has_matches = 1;
        println("Agente: Ha puesto papel y fósforos");
        pthread_cond_signal(&cond_smoker_tobacco);
        break;
      case 1:
        has_tobacco = 1;
        has_matches = 1;
        println("Agente: Ha puesto tabaco y fósforos");
        pthread_cond_signal(&cond_smoker_paper);
        break;
      case 2:
        has_tobacco = 1;
        has_paper = 1;
        println("Agente: Ha puesto tabaco y papel");
        pthread_cond_signal(&cond_smoker_matches);
        break;
    }

    pthread_mutex_unlock(&mutex);

    sleep(rand() % 3 + 1);
  }
  return NULL;
}

int main(int argc, char* argv[]) {
  pthread_t agent_thread;
  pthread_t tobacco_thread, paper_thread, matches_thread;

  if (pthread_create(&agent_thread, NULL, smokers_agent, NULL) != 0)
    error("error thread_agent");

  if (pthread_create(&tobacco_thread, NULL, smoker_tobacco, NULL) != 0) 
    error("errror thread_tobacco");

  if (pthread_create(&paper_thread, NULL, smoker_paper, NULL) != 0)
    error("error thread_paper");

  if (pthread_create(&matches_thread, NULL, smoker_matches, NULL) != 0)
    error("error thread_matches");

  pthread_join(agent_thread, NULL);
  pthread_join(tobacco_thread, NULL);
  pthread_join(paper_thread, NULL);
  pthread_join(matches_thread, NULL);

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond_smoker_tobacco);
  pthread_cond_destroy(&cond_smoker_paper);
  pthread_cond_destroy(&cond_smoker_matches);

  return EXIT_SUCCESS;
}
