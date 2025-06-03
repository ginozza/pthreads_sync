#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int m, n;
double **A;
double *B;
double *C;

int next_row_to_process = 0;
pthread_mutex_t row_mutex;
pthread_cond_t work_cond;
int no_more_work = 0;

int rows_completed = 0;
pthread_cond_t all_work_done_cond;

void *multiply_rows_recycled(void *arg) {
    long thread_id = (long)arg;
    int current_row;

    while (1) {
        pthread_mutex_lock(&row_mutex);

        while (next_row_to_process >= m && !no_more_work) {
            pthread_cond_wait(&work_cond, &row_mutex);
        }

        if (no_more_work) {
            pthread_mutex_unlock(&row_mutex);
            break;
        }

        current_row = next_row_to_process;
        next_row_to_process++;
        
        pthread_mutex_unlock(&row_mutex);

        printf("Hilo %ld procesando fila %d\n", thread_id, current_row);
        C[current_row] = 0.0;
        for (int j = 0; j < n; j++) {
            C[current_row] += A[current_row][j] * B[j];
        }

        pthread_mutex_lock(&row_mutex);
        rows_completed++;
        if (rows_completed == m) {
            pthread_cond_signal(&all_work_done_cond);
        }
        pthread_mutex_unlock(&row_mutex);
    }
    pthread_exit(NULL);
}

void *calculate_specific_component(void *arg) {
    int component_index = *(int *)arg;
    if (component_index >= 0 && component_index < m) {
        printf("Hilo especifico calculando C[%d]\n", component_index);
        double sum = 0.0;
        for (int j = 0; j < n; j++) {
            sum += A[component_index][j] * B[j];
        }
        C[component_index] = sum;
    }
    pthread_exit(NULL);
}

int main() {
    srand(time(NULL));

    printf("Ingrese el numero de filas (m) de la matriz A: ");
    scanf("%d", &m);
    printf("Ingrese el numero de columnas (n) de la matriz A y filas del vector B: ");
    scanf("%d", &n);

    A = (double **)malloc(m * sizeof(double *));
    for (int i = 0; i < m; i++) {
        A[i] = (double *)malloc(n * sizeof(double));
    }
    B = (double *)malloc(n * sizeof(double));
    C = (double *)malloc(m * sizeof(double));

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            A[i][j] = (double)rand() / RAND_MAX * 10.0;
        }
    }

    for (int i = 0; i < n; i++) {
        B[i] = (double)rand() / RAND_MAX * 10.0;
    }

    printf("Matriz A:\n");
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            printf("%.2f ", A[i][j]);
        }
        printf("\n");
    }

    printf("\nVector B:\n");
    for (int i = 0; i < n; i++) {
        printf("%.2f\n", B[i]);
    }

    int num_threads;
    printf("\nIngrese el numero de hilos a crear: ");
    scanf("%d", &num_threads);

    pthread_mutex_init(&row_mutex, NULL);
    pthread_cond_init(&work_cond, NULL);
    pthread_cond_init(&all_work_done_cond, NULL);

    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, multiply_rows_recycled, (void *)(long)i);
    }

    pthread_t specific_thread;
    int specific_component_index = m;
    if (m > 0) {
        specific_component_index = (m - 1); 
    } else {
        specific_component_index = 0; 
    }
    
    pthread_create(&specific_thread, NULL, calculate_specific_component, (void *)&specific_component_index);

    pthread_mutex_lock(&row_mutex);
    while (rows_completed < m) {
        pthread_cond_wait(&all_work_done_cond, &row_mutex);
    }
    no_more_work = 1; 
    pthread_cond_broadcast(&work_cond); 
    pthread_mutex_unlock(&row_mutex);

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(specific_thread, NULL);

    pthread_mutex_destroy(&row_mutex);
    pthread_cond_destroy(&work_cond);
    pthread_cond_destroy(&all_work_done_cond);

    printf("\nVector Resultante C:\n");
    for (int i = 0; i < m; i++) {
        printf("C[%d] = %.2f\n", i, C[i]);
    }

    for (int i = 0; i < m; i++) {
        free(A[i]);
    }
    free(A);
    free(B);
    free(C);
    free(threads);

    return 0;
}
