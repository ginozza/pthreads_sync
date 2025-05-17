#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <GLFW/glfw3.h>

#define MAX_LINE 256
#define MAX_FILE_NAME 256
#define MAX_ZONES 16  // límite razonable para demo

typedef struct {
  char filename[MAX_FILE_NAME];
  char zone_name[40];
  double *temperatures;  // arreglo dinámico con temperaturas
  int count;             // cantidad de datos
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
  fgets(buffer_line, MAX_LINE, file);  // saltar header si existe

  data->temperatures = NULL;
  data->count = 0;

  while (fgets(buffer_line, MAX_LINE, file)) {
    char *token = strtok(buffer_line, ",");  // fecha ignorada
    token = strtok(NULL, ",");                // temperatura
    if (token) {
      double temp = atof(token);
      data->temperatures = realloc(data->temperatures, (data->count + 1) * sizeof(double));
      if (!data->temperatures) error("realloc failed");
      data->temperatures[data->count] = temp;
      data->count++;
    }
  }
  fclose(file);

  // calcular promedio
  double add = 0.0;
  for (int i = 0; i < data->count; i++) add += data->temperatures[i];
  data->mean = add / data->count;

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
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  pthread_exit(NULL);
}

void draw_graph(StationData *data, int n_files) {
  if (!glfwInit()) error("Failed to initialize GLFW");

  GLFWwindow* window = glfwCreateWindow(800, 600, "Temperature Zones", NULL, NULL);
  if (!window) {
    glfwTerminate();
    error("Failed to create GLFW window");
  }

  glfwMakeContextCurrent(window);

  // Configuración básica OpenGL
  glClearColor(1.0, 1.0, 1.0, 1.0);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 100, -10, 50, -1, 1); // Ajusta según máximo conteo y rango temp

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Colores para hasta 16 zonas
  float colors[MAX_ZONES][3] = {
    {1,0,0}, {0,1,0}, {0,0,1}, {1,1,0}, {1,0,1}, {0,1,1},
    {0.5,0,0}, {0,0.5,0}, {0,0,0.5}, {0.5,0.5,0}, {0.5,0,0.5}, {0,0.5,0.5},
    {0.7,0.3,0}, {0.3,0.7,0}, {0,0.3,0.7}, {0.7,0,0.3}
  };

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    // Dibujar cada zona
    for (int z = 0; z < n_files; z++) {
      glColor3fv(colors[z % MAX_ZONES]);
      glBegin(GL_LINE_STRIP);
      for (int i = 0; i < data[z].count; i++) {
        // X: tiempo (índice), Y: temperatura
        // Normaliza X a rango [0..100] para ventana 800 px ancho
        float x = ((float)i / (data[z].count > 1 ? (data[z].count - 1) : 1)) * 100.0f;
        float y = (float)data[z].temperatures[i];
        glVertex2f(x, y);
      }
      glEnd();
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

int main(int argc, char *argv[]) {
  if (argc < 2) error("uso: archivo1.txt archivo2.txt...");

  int n_files = argc - 1;
  if (n_files > MAX_ZONES) error("Demasiados archivos, max 16");

  pthread_t* threads = malloc(n_files * sizeof(pthread_t));
  StationData* data = malloc(n_files * sizeof(StationData));
  if (!threads || !data) error("malloc failed");

  for (int i = 0; i < n_files; ++i) {
    strncpy(data[i].filename, argv[i + 1], sizeof(data[i].filename));
    snprintf(data[i].zone_name, sizeof(data[i].zone_name), "Zona %d", i + 1);
    data[i].mean = 0.0;
    data[i].temperatures = NULL;
    data[i].count = 0;
    data[i].thread_index = i;

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
    printf("%s: Promedio = %.2f°C (Datos: %d)\n", data[i].zone_name, data[i].mean, data[i].count);
  printf("Zona más cálida: %s (%.2f°C)\n", max_zone, max_mean);
  printf("Zona menos cálida: %s (%.2f°C)\n", min_zone, min_mean);

  draw_graph(data, n_files);

  // Liberar memoria
  for (int i = 0; i < n_files; i++)
    free(data[i].temperatures);
  free(threads);
  free(data);

  return EXIT_SUCCESS;
}

