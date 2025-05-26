#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

int n_waiting_chairs;
int n_waiting_customers = 0;
int barber_is_sleeping = 1;
int customer_id_counter = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_customer_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_barber_ready = PTHREAD_COND_INITIALIZER;

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

void* barber(void* __) {
  while (1) {
    pthread_mutex_lock(&mutex);

    // if there isn't customers barber go to sleep
    if (n_waiting_customers == 0) {
      barber_is_sleeping = 1;
      println("Barbero: No hay clientes, me duermo");
      pthread_cond_wait(&cond_customer_ready, &mutex);
    }

    // there is clients, barber wake up
    barber_is_sleeping = 0;
    n_waiting_customers--; // a client moves from the waiting room to the chair 
    println("Barbero: Listo para atender cliente. En espera: %d", n_waiting_customers);

    // a chair in the barber room was freed
    pthread_cond_signal(&cond_barber_ready);

    pthread_mutex_unlock(&mutex);

    println("Barbero: Cortando el pelo...");
    sleep(5);

    println("Barbero: Ha terminado de cortar el pelo");
  }
  return NULL;
}

void* customer(void* arg) {
  int customer_id = (int)(intptr_t)arg;
  println("Cliente %d: Ha llegado a la barberia", customer_id);

  pthread_mutex_lock(&mutex);

  if (n_waiting_customers < n_waiting_chairs) {
    n_waiting_customers++;
    println("Cliente %d: Se sienta en una silla de espera", customer_id);

    if (barber_is_sleeping) {
      println("Cliente %d: Despierta al barbero", customer_id);
      pthread_cond_signal(&cond_customer_ready);
    }

    // customer has secured a chair
    pthread_mutex_unlock(&mutex);

    println("Cliente %d: Ha sido atendido, abandona el local", customer_id);
  } else {
    println("Cliente %d: No hay sillas dispomibles, abandona el local", customer_id);
    pthread_mutex_unlock(&mutex);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 2) error("Usage: <n_waiting_chairs>");

  n_waiting_chairs = atoi(argv[1]);

  srand(time(NULL));

  pthread_t barber_thread;
  if (pthread_create(&barber_thread, NULL, barber, NULL) != 0)
    error("error barber_thread");

  while (1) {
    int current_customer_id;
    pthread_mutex_lock(&mutex);
    customer_id_counter++;
    current_customer_id = customer_id_counter;
    pthread_mutex_unlock(&mutex);

    pthread_t customer_thread;
    if (pthread_create(&customer_thread, NULL, customer, (void*)(intptr_t)current_customer_id)) {
      error("error customer_thread");
    }

    // customers are infinite, they will never pthread_join
    // so we release their resources as soon as they finish
    // consuming resources
    pthread_detach(customer_thread);

    // simulate customer arrival
    sleep(rand() % 3 + 1);
  }

  pthread_join(barber_thread, NULL);

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond_customer_ready);
  pthread_cond_destroy(&cond_barber_ready);

  return EXIT_SUCCESS;
}
