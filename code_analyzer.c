#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 85
#define BUFFER_LINES 3

char *buffer_lines[BUFFER_LINES];
int lines_in_buffer;
int current_line = 0;
int terminated = 0;

int total_lines = 0;
int total_comments = 0;
int total_keywords = 0;

const char *keywords[] = {"int", "float", "char",   "if",     "else",
                          "for", "while", "return", "switch", "case"};
const int n_keywords = sizeof(keywords) / sizeof(keywords[0]);
const char *token_delimiters = " \t\n\r;(){}[]<>=+-*/%!&|,.\"'";

pthread_barrier_t barrier_start;
pthread_barrier_t barrier_end;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(const char *err) {
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

int has_comment_inline(const char *line) {
  int in_string = 0;

  for (const char *p = line; *p; ++p) {
    if (*p == '"' && (p == line || *(p - 1) != '\\')) {
      in_string = !in_string; 
    }
    if (!in_string && *p == '/' && *(p + 1) == '/') {
      return 1; 
    }
  }
  return 0;
}

int is_comment_line(const char *line) {
  while (*line == ' ' || *line == '\t')
    line++;
  return strncmp(line, "//", 2) == 0;
}

int line_contains_keyword(const char *line, const char *keyword) {
  char line_copy[MAX_LINE];
  strncpy(line_copy, line, MAX_LINE);
  line_copy[MAX_LINE - 1] = '\0'; 

  char *token = strtok(line_copy, token_delimiters);
  while (token != NULL) {
    if (strcmp(token, keyword) == 0)
      return 1;
    token = strtok(NULL, token_delimiters);
  }
  return 0;
}

void *consumer_lines(void *_) {
  while (1) {
    pthread_barrier_wait(&barrier_start);

    pthread_mutex_lock(&buffer_mutex);
    if (terminated) {
      pthread_mutex_unlock(&buffer_mutex);
      pthread_barrier_wait(&barrier_end);
      break;
    }

    for (int i = 0; i < lines_in_buffer; ++i) {
      printf("[Líneas]  Línea %d: %s", current_line + i + 1, buffer_lines[i]);
      total_lines++;
    }

    pthread_mutex_unlock(&buffer_mutex);
    pthread_barrier_wait(&barrier_end);
  }
  return NULL;
}

void *consumer_comments(void *_) {
  while (1) {
    pthread_barrier_wait(&barrier_start);

    pthread_mutex_lock(&buffer_mutex);
    if (terminated) {
      pthread_mutex_unlock(&buffer_mutex);
      pthread_barrier_wait(&barrier_end);
      break;
    }

    for (int i = 0; i < lines_in_buffer; ++i) {
      if (is_comment_line(buffer_lines[i]) ||
          has_comment_inline(buffer_lines[i])) {
        printf("[Comentario]  Linea: %d: %s", current_line + i + 1,
               buffer_lines[i]);
        total_comments++;
      }
    }

    pthread_mutex_unlock(&buffer_mutex);
    pthread_barrier_wait(&barrier_end);
  }
  return NULL;
}

void *consumer_keywords(void *_) {
  while (1) {
    pthread_barrier_wait(&barrier_start);

    pthread_mutex_lock(&buffer_mutex);
    if (terminated) {
      pthread_mutex_unlock(&buffer_mutex);
      pthread_barrier_wait(&barrier_end);
      break;
    }

    for (int i = 0; i < lines_in_buffer; ++i) {
      for (int k = 0; k < n_keywords; ++k) {
        if (line_contains_keyword(buffer_lines[i], keywords[k])) {
          printf("[Palabras Clave] Linea %d contiene '%s'\n",
                 current_line + i + 1, keywords[k]);
          total_keywords++;
        }
      }
    }

    pthread_mutex_unlock(&buffer_mutex);
    pthread_barrier_wait(&barrier_end);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    error("incorrect format usage");

  FILE *file = fopen(argv[1], "r");
  if (!file)
    error("error opening file");

  pthread_barrier_init(&barrier_start, NULL, 4);
  pthread_barrier_init(&barrier_end, NULL, 4);

  pthread_t thread_lines, thread_comments, thread_keywords;
  pthread_create(&thread_lines, NULL, consumer_lines, NULL);
  pthread_create(&thread_comments, NULL, consumer_comments, NULL);
  pthread_create(&thread_keywords, NULL, consumer_keywords, NULL);

  char line[MAX_LINE];
  while (1) {
    pthread_mutex_lock(&buffer_mutex);

    lines_in_buffer = 0;
    for (int i = 0; i < BUFFER_LINES; ++i) {
      if (fgets(line, sizeof(line), file)) {
        buffer_lines[i] = strdup(line);
        lines_in_buffer++;
      } else {
        break;
      }
    }

    if (lines_in_buffer == 0) {
      terminated = 1;
      pthread_mutex_unlock(&buffer_mutex);
      pthread_barrier_wait(&barrier_start);
      pthread_barrier_wait(&barrier_end);
      break;
    }

    pthread_mutex_unlock(&buffer_mutex);

    pthread_barrier_wait(&barrier_start);
    pthread_barrier_wait(&barrier_end);

    for (int i = 0; i < lines_in_buffer; ++i) {
      free(buffer_lines[i]);
    }

    current_line += lines_in_buffer;
  }

  pthread_join(thread_lines, NULL);
  pthread_join(thread_comments, NULL);
  pthread_join(thread_keywords, NULL);

  fclose(file);

  pthread_barrier_destroy(&barrier_start);
  pthread_barrier_destroy(&barrier_end);
  pthread_mutex_destroy(&buffer_mutex);

  println("\n ------- Resultados ------");
  println("Conteo total de lineas:                  %d", total_lines);
  println("Conteo total de lineas con comentarios:  %d", total_comments);
  println("Conteo total de palabras clave:          %d", total_keywords);

  return EXIT_SUCCESS;
}
