#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_STATIONS 4

typedef enum { BATERIA, MOTOR, DIRECCION, NAVEGACION } TipoMantenimiento;

const char *mantenimiento_nombres[] = {"Bateria", "Motor", "Direccion",
                                       "Sistema de navegacion"};

typedef struct {
  int id;
  TipoMantenimiento tareas[NUM_STATIONS];
  int tareas_completadas;
  int tarea_actual_idx;
} Vehiculo;

typedef struct {
  int id;
  int capacidad;
  pthread_mutex_t mutex_estacion;
  pthread_cond_t cond_estacion;
  int vehiculos_en_estacion;
} EstacionMantenimiento;

int num_vehiculos;
int capacidad_estacion;
Vehiculo *vehiculos;
EstacionMantenimiento estaciones[NUM_STATIONS];

pthread_mutex_t mutex_impresion;
pthread_mutex_t mutex_vehiculos_listos;
int vehiculos_listos_para_carretera = 0;

void *hilo_vehiculo(void *arg) {
  Vehiculo *vehiculo = (Vehiculo *)arg;

  pthread_mutex_lock(&mutex_impresion);
  printf("Vehiculo %d ingresado\n", vehiculo->id);
  pthread_mutex_unlock(&mutex_impresion);

  while (vehiculo->tareas_completadas < NUM_STATIONS) {
    int estacion_elegida = -1;
    int ingresado = 0;

    while (!ingresado) {
      for (int i = 0; i < NUM_STATIONS; ++i) {
        int tarea_ya_hecha = 0;
        for (int j = 0; j < vehiculo->tareas_completadas; ++j) {
          if (vehiculo->tareas[j] == i) {
            tarea_ya_hecha = 1;
            break;
          }
        }
        if (tarea_ya_hecha) {
          continue;
        }

        pthread_mutex_lock(&estaciones[i].mutex_estacion);
        if (estaciones[i].vehiculos_en_estacion < estaciones[i].capacidad) {
          estaciones[i].vehiculos_en_estacion++;
          estacion_elegida = i;
          ingresado = 1;
          pthread_mutex_unlock(&estaciones[i].mutex_estacion);
          break;
        }
        pthread_mutex_unlock(&estaciones[i].mutex_estacion);
      }

      if (!ingresado) {
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

    usleep(rand() % 1000000 + 500000);

    pthread_mutex_lock(&mutex_impresion);
    printf("Vehiculo %d ha completado el mantenimiento de la %s en la estacion "
           "de mantenimiento %d.\n",
           vehiculo->id, mantenimiento_nombres[estacion_elegida],
           estacion_elegida + 1);
    pthread_mutex_unlock(&mutex_impresion);

    vehiculo->tareas[vehiculo->tareas_completadas] = estacion_elegida;
    vehiculo->tareas_completadas++;

    pthread_mutex_lock(&estaciones[estacion_elegida].mutex_estacion);
    estaciones[estacion_elegida].vehiculos_en_estacion--;
    pthread_cond_signal(&estaciones[estacion_elegida].cond_estacion);
    pthread_mutex_unlock(&estaciones[estacion_elegida].mutex_estacion);
  }

  pthread_mutex_lock(&mutex_vehiculos_listos);
  vehiculos_listos_para_carretera++;
  pthread_mutex_unlock(&mutex_vehiculos_listos);

  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Uso: %s <numero_de_vehiculos> <capacidad_estacion>\n",
            argv[0]);
    return 1;
  }

  num_vehiculos = atoi(argv[1]);
  capacidad_estacion = atoi(argv[2]);

  if (num_vehiculos <= 0 || capacidad_estacion <= 0) {
    fprintf(stderr, "El numero de vehiculos y la capacidad de la estacion "
                    "deben ser mayores que cero.\n");
    return 1;
  }

  vehiculos = (Vehiculo *)malloc(num_vehiculos * sizeof(Vehiculo));
  if (vehiculos == NULL) {
    perror("Error al asignar memoria para vehiculos");
    return 1;
  }
  for (int i = 0; i < num_vehiculos; ++i) {
    vehiculos[i].id = i + 1;
    vehiculos[i].tareas_completadas = 0;
    vehiculos[i].tarea_actual_idx = 0;
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
  if (pthread_mutex_init(&mutex_vehiculos_listos, NULL) != 0) {
    perror("Error al inicializar el mutex de vehiculos listos");
    return 1;
  }

  pthread_t *hilos_vehiculos =
      (pthread_t *)malloc(num_vehiculos * sizeof(pthread_t));
  if (hilos_vehiculos == NULL) {
    perror("Error al asignar memoria para hilos de vehiculos");
    return 1;
  }

  for (int i = 0; i < num_vehiculos; ++i) {
    if (pthread_create(&hilos_vehiculos[i], NULL, hilo_vehiculo,
                       (void *)&vehiculos[i]) != 0) {
      perror("Error al crear hilo de vehiculo");
      return 1;
    }
  }

  for (int i = 0; i < num_vehiculos; ++i) {
    if (pthread_join(hilos_vehiculos[i], NULL) != 0) {
      perror("Error al esperar por hilo de vehiculo");
      return 1;
    }
  }

  pthread_mutex_lock(&mutex_impresion);
  printf("Todos los vehiculos han completado su mantenimiento y estan listos "
         "para volver a la carretera.\n");
  pthread_mutex_unlock(&mutex_impresion);

  free(hilos_vehiculos);
  free(vehiculos);
  for (int i = 0; i < NUM_STATIONS; ++i) {
    pthread_mutex_destroy(&estaciones[i].mutex_estacion);
    pthread_cond_destroy(&estaciones[i].cond_estacion);
  }
  pthread_mutex_destroy(&mutex_impresion);
  pthread_mutex_destroy(&mutex_vehiculos_listos);

  return 0;
}
