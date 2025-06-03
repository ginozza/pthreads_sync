#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_STATIONS 4
#define GENERATION_INTERVAL_SECONDS 1

typedef enum { BATERIA, MOTOR, DIRECCION, NAVEGACION } TipoMantenimiento;

const char *mantenimiento_nombres[] = {"Bateria", "Motor", "Direccion",
                                       "Sistema de navegacion"};

typedef struct {
  int id;
  TipoMantenimiento tareas_pendientes[NUM_STATIONS];
  bool tarea_completada[NUM_STATIONS];
  int tareas_restantes;
} Vehiculo;

typedef struct {
  int id;
  int capacidad;
  pthread_mutex_t mutex_estacion;
  pthread_cond_t cond_estacion;
  int vehiculos_en_estacion;
} EstacionMantenimiento;

int capacidad_estacion;
EstacionMantenimiento estaciones[NUM_STATIONS];
pthread_mutex_t mutex_impresion;
int next_vehiculo_id = 1;

void *hilo_vehiculo(void *arg) {
  Vehiculo *vehiculo = (Vehiculo *)arg;

  pthread_mutex_lock(&mutex_impresion);
  printf("Vehiculo %d ha ingresado al centro de servicio.\n", vehiculo->id);
  pthread_mutex_unlock(&mutex_impresion);

  for (int i = 0; i < NUM_STATIONS; ++i) {
    vehiculo->tareas_pendientes[i] = (TipoMantenimiento)i;
    vehiculo->tarea_completada[i] = false;
  }
  vehiculo->tareas_restantes = NUM_STATIONS;

  while (vehiculo->tareas_restantes > 0) {
    int estacion_elegida = -1;
    bool ingresado_a_estacion = false;

    while (!ingresado_a_estacion) {
      for (int i = 0; i < NUM_STATIONS; ++i) {
        if (vehiculo->tarea_completada[i]) {
          continue;
        }

        pthread_mutex_lock(&estaciones[i].mutex_estacion);
        if (estaciones[i].vehiculos_en_estacion < estaciones[i].capacidad) {
          estaciones[i].vehiculos_en_estacion++;
          estacion_elegida = i;
          ingresado_a_estacion = true;
          pthread_mutex_unlock(&estaciones[i].mutex_estacion);
          break;
        }
        pthread_mutex_unlock(&estaciones[i].mutex_estacion);
      }

      if (!ingresado_a_estacion) {
        pthread_mutex_lock(&mutex_impresion);
        printf("Vehiculo %d esta esperando para ingresar a alguna estacion de "
               "mantenimiento.\n",
               vehiculo->id);
        pthread_mutex_unlock(&mutex_impresion);
        usleep(100000);
      }
    }

    pthread_mutex_lock(&mutex_impresion);
    printf("Vehiculo %d ha ingresado a la estacion de mantenimiento %d.\n",
           vehiculo->id, estacion_elegida + 1);
    printf("Vehiculo %d ha comenzado el mantenimiento del %s en la estacion de "
           "mantenimiento %d.\n",
           vehiculo->id, mantenimiento_nombres[estacion_elegida],
           estacion_elegida + 1);
    pthread_mutex_unlock(&mutex_impresion);

    usleep(rand() % 2000000 + 500000);

    pthread_mutex_lock(&mutex_impresion);
    printf("Vehiculo %d ha completado el mantenimiento de la %s en la estacion "
           "de mantenimiento %d.\n",
           vehiculo->id, mantenimiento_nombres[estacion_elegida],
           estacion_elegida + 1);
    pthread_mutex_unlock(&mutex_impresion);

    vehiculo->tarea_completada[estacion_elegida] = true;
    vehiculo->tareas_restantes--;

    pthread_mutex_lock(&estaciones[estacion_elegida].mutex_estacion);
    estaciones[estacion_elegida].vehiculos_en_estacion--;
    pthread_cond_signal(&estaciones[estacion_elegida].cond_estacion);
    pthread_mutex_unlock(&estaciones[estacion_elegida].mutex_estacion);
  }

  pthread_mutex_lock(&mutex_impresion);
  printf("Vehiculo %d ha completado todas sus tareas de mantenimiento y esta "
         "listo para volver a la carretera.\n",
         vehiculo->id);
  pthread_mutex_unlock(&mutex_impresion);

  free(vehiculo);
  pthread_exit(NULL);
}

void *hilo_generador_vehiculos(void *arg) {
  while (true) {
    Vehiculo *nuevo_vehiculo = (Vehiculo *)malloc(sizeof(Vehiculo));
    if (nuevo_vehiculo == NULL) {
      perror("Error al asignar memoria para nuevo vehiculo");
      continue;
    }
    nuevo_vehiculo->id = next_vehiculo_id++;

    pthread_t vehiculo_thread;
    if (pthread_create(&vehiculo_thread, NULL, hilo_vehiculo,
                       (void *)nuevo_vehiculo) != 0) {
      perror("Error al crear hilo de vehiculo");
      free(nuevo_vehiculo);
    } else {
      pthread_detach(vehiculo_thread);
    }
    sleep(GENERATION_INTERVAL_SECONDS);
  }
  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Uso: %s <capacidad_estacion>\n", argv[0]);
    return 1;
  }

  capacidad_estacion = atoi(argv[1]);

  if (capacidad_estacion <= 0) {
    fprintf(stderr, "La capacidad de la estacion debe ser mayor que cero.\n");
    return 1;
  }

  for (int i = 0; i < NUM_STATIONS; ++i) {
    estaciones[i].id = i + 1;
    estaciones[i].capacidad = capacidad_estacion;
    estaciones[i].vehiculos_en_estacion = 0;
    if (pthread_mutex_init(&estaciones[i].mutex_estacion, NULL) != 0) {
      perror("Error al inicializar el mutex de la estacion");
      return 1;
    }
    if (pthread_cond_init(&estaciones[i].cond_estacion, NULL) != 0) {
      perror("Error al inicializar la variable de condicion de la estacion");
      return 1;
    }
  }

  if (pthread_mutex_init(&mutex_impresion, NULL) != 0) {
    perror("Error al inicializar el mutex de impresion");
    return 1;
  }

  pthread_t generador_thread;
  if (pthread_create(&generador_thread, NULL, hilo_generador_vehiculos, NULL) !=
      0) {
    perror("Error al crear hilo generador de vehiculos");
    return 1;
  }

  pthread_join(generador_thread, NULL);

  for (int i = 0; i < NUM_STATIONS; ++i) {
    pthread_mutex_destroy(&estaciones[i].mutex_estacion);
    pthread_cond_destroy(&estaciones[i].cond_estacion);
  }
  pthread_mutex_destroy(&mutex_impresion);

  return 0;
}
