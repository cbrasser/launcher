#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 100
#define MAX_NUM_RESULTS 100
#define MAX_RESULT_LENGTH 100
#define BASE_PATH "~/"
#define DELIMITER "/"
/*
 *------------------I/O related stuff------------------
 */

typedef enum { ERROR_INPUT_TOO_LONG } Errors;
typedef enum { RESULT_FOUND, RESULT_NOT_FOUND } Result;

typedef struct {
  char *input_buffer;
  char **result_buffer;

} thread_args;

char **create_result_buffer() {
  char **b = malloc(MAX_NUM_RESULTS * sizeof(char *));
  for (int i = 0; i < MAX_NUM_RESULTS; i++) {
    b[i] = malloc(MAX_RESULT_LENGTH * sizeof(char));
  }
  return b;
}

void clear_result_buffer(char **b) {
  for (int i = 0; i < MAX_NUM_RESULTS; i++) {
    b[i][0] = '\t';
  }
}

void find_file(char *input_buffer, char **result_buffer) {

  char command[120];
  strcpy(command, "find ");
  strcat(command, BASE_PATH);
  strcat(command, " -name ");
  strcat(command, input_buffer);
  strcat(command, " 2>&1");
  FILE *fp = popen(command, "r");
  char temp_buf[1024];
  // Get output of find command
  int i = 0;
  while (fgets(temp_buf, sizeof(temp_buf), fp) != 0 && i < MAX_NUM_RESULTS) {
    strncpy(result_buffer[i], temp_buf, strlen(temp_buf)-1);
    i += 1;
  }
  // Close stream from stdin
  pclose(fp);
  return; 
}

void open_file(char location[], char filename[], char filetype[]) {
  char full_path[80];
  char command[100];
  strcpy(full_path, location);
  strcat(full_path, filename);
  strcat(full_path, ".");
  strcat(full_path, filetype);
  strcpy(command, "mupdf ");
  strcat(command, full_path);
  system(command);
}

void search_loop(void *t) {
  char *last_buffer = malloc(sizeof(char) * MAX_INPUT_LENGTH);
  thread_args *t_a = (thread_args *)t;
  while (true) {
    // First iteration
    if (last_buffer[0] == '\0' && t_a->input_buffer[0] != '\0') {
      strcpy(last_buffer, t_a->input_buffer);
      find_file(t_a->input_buffer, t_a->result_buffer);
      // New search term
    } else if (strcmp(last_buffer, t_a->input_buffer) != 0) {
      clear_result_buffer(t_a->result_buffer);
      last_buffer[0] = '\0';
      strcpy(last_buffer, t_a->input_buffer);
      find_file(t_a->input_buffer, t_a->result_buffer);
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
    if (c == ' ') {
      return;
    }
    // check if input is not to long
    if (i == MAX_INPUT_LENGTH) {
      return;
    }
    // Get the latest added char and add to buffer
    t_a->input_buffer[i] = c;
    i += 1;
  }
}

void gui_loop(void *t){
  thread_args *t_a = (thread_args *)t;
  while(true) {
    printf("\r \033[1;36m> \033[0m%s \033[0;33m%s\033[0m", t_a->input_buffer, t_a->result_buffer[0]);
    fflush(stdout);
    sleep(1);
  }
}

// main loop, reads input, currently only recognized '.exit' as a valid command
int main(int argc, char *argv[]) {

  char *input_buffer = malloc(sizeof(char) * MAX_INPUT_LENGTH);
  char **result_buffer = create_result_buffer();

  thread_args *t_a = malloc(sizeof(thread_args));
  t_a->input_buffer = input_buffer;
  t_a->result_buffer = result_buffer;


  pthread_t t_id_search, t_id_input, t_id_gui;
  pthread_create(&t_id_search, NULL, search_loop, (void *)t_a);
  pthread_create(&t_id_input, NULL, input_loop, (void *)t_a);
  pthread_create(&t_id_gui, NULL, gui_loop, (void *)t_a);

  pthread_join(t_id_input, NULL); 
  pthread_cancel(t_id_search);
  pthread_cancel(t_id_gui);

  return 0;
}
