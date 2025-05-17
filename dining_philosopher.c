#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdarg.h>

#define NP 5
#define THINKING 2
#define HUNGRY 1
#define EATING 0
#define LEFT (ph_id + (NP - 1)) % NP 
#define RIGHT (ph_id + (NP - 4)) % NP
#define LEFTP (LEFT + 1)
#define RIGHTP (RIGHT + 1)
#define ACTUALP (ph_id + 1)

int state[NP];
int ph[NP] = {0, 1, 2, 3, 4};

sem_t mutex;
sem_t S[NP];

void println(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  
  putchar('\n');
}

void test(int ph_id) {
  if (state[ph_id] == HUNGRY 
      && state[LEFT] != EATING
      && state[RIGHT] != EATING) {
    state[ph_id] = EATING;

    sleep(2);

    println("Filósofo %d toma los comensales %d y %d",
           ACTUALP, LEFTP, RIGHTP);

    println("Filósofo %d está Comiendo",
           ACTUALP);

    sem_post(&S[ph_id]);
  }
}

void take_fork(int ph_id) {
  sem_wait(&mutex);

  state[ph_id] = HUNGRY;

  println("Filósofo %d está Hambriento", 
          ACTUALP);

  // Eat if neighbours aren't eating
  test(ph_id);

  sem_post(&mutex);

  // If unable to eat wait signal
  sem_wait(&S[ph_id]);

  sleep(1);
}

void put_fork(int ph_id) {
  sem_wait(&mutex);

  state[ph_id] = THINKING;

  println("Filósofo %d dejó los comensales %d y %d",
          ACTUALP, LEFTP, RIGHTP);

  println("Filósofo %d está pensando",
          ACTUALP);

  test(LEFT);
  test(RIGHT);

  sem_post(&mutex);
}

void* philosopher(void *id) {
  while(1) {
    int* i = id;
    
    sleep(1);
    
    take_fork(*i);
    
    sleep(0); // Yields the CPU turn if another thread can run
    
    put_fork(*i);
  }
}

int main() {
  int i;
  pthread_t thread_id[5];

  sem_init(&mutex, 0, 1); // Init mutex for threads (0), starts unlocked (1)

  for (i = 0; i < NP; ++i)
    sem_init(&S[i], 0, 0);

  for (i = 0; i < NP; ++i) {
    pthread_create(&thread_id[i], NULL, philosopher, &ph[i]);

    println("Filósofo %d está pensando", i + 1);
  }

  for (i = 0; i < NP; ++i)
    pthread_join(thread_id[i], NULL);

  return 0;
}
