#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../headers/constants.h"
#include "../headers/file_handlers.h"
#include "../headers/search.h"
#include "../headers/suggestion_logic.h"

pthread_mutex_t input_lock;
pthread_mutex_t suggestion_lock;

//TODO: Split into multiple files and clean up
/*
 *------------------I/O related stuff------------------
 */

typedef enum { ERROR_INPUT_TOO_LONG } Errors;
typedef enum { RESULT_FOUND, RESULT_NOT_FOUND } Result;

typedef struct {
  char *input_buffer;
  char **result_buffer;
  char *current_suggestion;
  int new_input;
  int new_suggestions;
  int suggestion_index;
} thread_args;

char **create_result_buffer() {
  char **b = malloc(MAX_NUM_RESULTS * sizeof(char *));
  for (int i = 0; i < MAX_NUM_RESULTS; i++) {
    b[i] = malloc(MAX_RESULT_LENGTH * sizeof(char));
    b[i][0] = '\0';
  }
  return b;
}

void delete_result_buffer(char **b) {
  for (int i = 0; i < MAX_NUM_RESULTS; i++) {
    free(b[i]);
  }
  free(b);
}

void clear_result_buffer(char **b) {
  for (int i = 0; i < MAX_NUM_RESULTS; i++) {
    b[i][0] = '\0';
  }
}


void search_loop(void *t) {
  thread_args *t_a = (thread_args *)t;
  while (true) {
    // Only search if we have a new input
    if (t_a->new_input) {
      pthread_mutex_lock(&input_lock);
      t_a->new_input = 0;
      pthread_mutex_unlock(&input_lock);
      clear_result_buffer(t_a->result_buffer);
      find_file(t_a->input_buffer, t_a->result_buffer);
      pthread_mutex_lock(&suggestion_lock);
      t_a->new_suggestions = 1;
      pthread_mutex_unlock(&suggestion_lock);
    }
  }
}

void input_loop(void *t) {
  thread_args *t_a = (thread_args *)t;
  int c;
  int i = 0;
  while (true) {
    system("/bin/stty raw");
    c = getchar();
    system("/bin/stty cooked");

    if (c == EXIT_KEY) {
      return;
    }
    if (c == 9 || c == 13) { // Tab/Enter
      if (strlen(t_a->current_suggestion) > 0) {
        if (open_file(t_a->current_suggestion)) {
          // Opened successfull
          return;
        }
      }
    } else if (c == 127) { // backspace
      if (i > 0) {
        t_a->input_buffer[i - 1] = '\0';
        i -= 1;
      }
    } else if ((c >= 97 && c <= 122) || c == 46 || c== 47 || c == 95 ||
               (c >= 65 && c <= 90) || (c >= 48 && c <= 57)) { // normal char
      t_a->input_buffer[i] = c;
      i += 1;
      // TODO: also consider special chars and uppercases
    }
    // check if input is not to long
    if (i == MAX_INPUT_LENGTH) {
      return;
    }

    // Notify search thread to re-calculate search
    pthread_mutex_lock(&input_lock);
    t_a->new_input = 1;
    pthread_mutex_unlock(&input_lock);
  }
}

void gui_loop(void *t) {
  thread_args *t_a = (thread_args *)t;
  double val = 0;
  while (true) {
    // Only update best suggestion if new suggestions available
    if (t_a->new_suggestions) {
      pthread_mutex_lock(&suggestion_lock);
      t_a->new_suggestions = 0;
      pthread_mutex_unlock(&suggestion_lock);
      t_a->current_suggestion = get_best_suggestion(t_a->result_buffer);
      val = get_suggestion_value(t_a->current_suggestion);
    }
    printf("\33[2K\r \033[1;36m> \033[0m%s \033[0;33m%s\033[0m",
           t_a->input_buffer, t_a->current_suggestion);
    fflush(stdout);
    // Wait for 100ms so the cursor doesnt go crazy
    usleep(50);
  }
}

// main loop
int main(int argc, char *argv[]) {

  thread_args *t_a = malloc(sizeof(thread_args));
  t_a->input_buffer = malloc(sizeof(char) * MAX_INPUT_LENGTH);
  t_a->current_suggestion = malloc(sizeof(char) * MAX_RESULT_LENGTH);
  t_a->result_buffer = create_result_buffer();
  t_a->new_input = 0;
  t_a->new_suggestions = 0;
  t_a->suggestion_index = 0;

  if (pthread_mutex_init(&input_lock, NULL) != 0) {
    printf("\n mutex init failed\n");
    return 1;
  }
  if (pthread_mutex_init(&suggestion_lock, NULL) != 0) {
    printf("\n mutex init failed\n");
    return 1;
  }

  pthread_t t_id_search, t_id_input, t_id_gui;
  pthread_create(&t_id_search, NULL, search_loop, (void *)t_a);
  pthread_create(&t_id_input, NULL, input_loop, (void *)t_a);
  pthread_create(&t_id_gui, NULL, gui_loop, (void *)t_a);

  pthread_join(t_id_input, NULL);
  pthread_cancel(t_id_search);
  pthread_cancel(t_id_gui);

  return 0;
}
