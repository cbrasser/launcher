#include <dirent.h>
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

#define MAX_INPUT_LENGTH 100
#define MAX_NUM_RESULTS 100
#define MAX_RESULT_LENGTH 1000
#define MAX_PATH_DEPTH 10
#define BASE_PATH "/home/clados"
#define DELIMITER '/'
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
    b[i][0] = '\0';
  }
  return b;
}

void delete_result_buffer(char** b){
  for (int i=0;i<MAX_NUM_RESULTS;i++){
    free(b[i]);
  }
  free(b);
}


void clear_result_buffer(char **b) {
  for (int i = 0; i < MAX_NUM_RESULTS; i++) {
    b[i][0] = '\0';
  }
}

int search_fs(char ***list, char *file_name, char *directory, int *location) {
  DIR *opened_dir;
  struct dirent *directory_structure;

  // printf("searching for '%s' in '%s'\n",file_name, directory);
  char *temp_dir = (char *)malloc(sizeof(char) * MAX_RESULT_LENGTH);
  if (!temp_dir)
    return -1;
  strcat(directory, "/");
  opened_dir = opendir(directory);
  if (opened_dir == NULL) {
    printf("could not open dir: %s\n", directory);
    free(temp_dir);
    return -1;
  }
  while (1) {
    directory_structure = readdir(opened_dir);
    if (!directory_structure) {
      closedir(opened_dir);
      free(temp_dir);
      return 0;
    }
    if (!strcmp(directory_structure->d_name, "..") ||
        !strcmp(directory_structure->d_name, "."))
      continue;

    else if (!strcmp(directory_structure->d_name, file_name)) {
      (*list)[*location] = (char *)malloc(sizeof(char) * MAX_RESULT_LENGTH);
      sprintf((*list)[*location], "%s%s", directory,
              directory_structure->d_name);
      (*location)++;
    } else if (directory_structure->d_type == DT_DIR) {
      sprintf(temp_dir, "%s%s", directory, directory_structure->d_name);
      if (search_fs(list, file_name, temp_dir, location) == -1) {
        closedir(opened_dir);
        free(temp_dir);
        return -1;
      }
    }
  }

  return 0;
}

int find_file(char *input_buffer, char **result_buffer) {
  char ** found_list = (char**)malloc(sizeof(char*) * MAX_NUM_RESULTS);
  if (!found_list) {
    printf("could not get result list\n");
    return 0;
  }

  int location_counter = 0;

  char *ini_dir = (char *)malloc(sizeof(char) * 500);
  if (!ini_dir) {
    free(found_list);
    return 0;
  }

  strcpy(ini_dir, BASE_PATH);
  search_fs(&found_list, input_buffer, ini_dir, &location_counter);
  if (location_counter > 0) {
    while (--location_counter >= 0) {
      strcpy(result_buffer[location_counter], found_list[location_counter]);
      free(found_list[location_counter]);
    }
  } else {
    //printf("Failed to find the file!\n");
  }

  free(ini_dir);
  free(found_list);

  return 1;
}


// Estimate the "value" of a possible search result
// to provide better suggestions
double get_suggestion_value(char *suggestion) {
  // Get path depth, TODO: is there a function for this ?
  double depth = 0;
  int i = 0;
  // printf("calcing value for: %s\n", suggestion);
  while (suggestion[i] != '\0') {
    // printf("%c",suggestion[i]);
    if (suggestion[i] == DELIMITER) {
      depth += 1;
    }
    i += 1;
  }
  depth /= MAX_PATH_DEPTH;
  return depth;
}

char *get_best_suggestion(char **result_buffer) {

  double best_suggestion_value = 0;
  int best_suggestion = 0;
  for (int i = 0; i < MAX_RESULT_LENGTH; i++) {
    // printf("checking suggestion %d - %s\n", i, result_buffer[i]);
    // Return if we reach an empty string in result_buffer
    if (result_buffer[i][0] == '\0') {
      // printf("is empty\n");
      return result_buffer[best_suggestion];
    }
    // printf("calc value of %s\n", result_buffer[i]);
    double val = get_suggestion_value(result_buffer[i]);
    if (val > best_suggestion_value) {
      best_suggestion = i;
      best_suggestion_value = val;
    }
  }
  return result_buffer[best_suggestion];
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
      int r = find_file(t_a->input_buffer, t_a->result_buffer);
      // New search term
    } else if (strcmp(last_buffer, t_a->input_buffer) != 0) {
      clear_result_buffer(t_a->result_buffer);
      last_buffer[0] = '\0';
      strcpy(last_buffer, t_a->input_buffer);
      int r = find_file(t_a->input_buffer, t_a->result_buffer);
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

void gui_loop(void *t) {
  thread_args *t_a = (thread_args *)t;
  char final_result[MAX_RESULT_LENGTH];
  double val = 0;
  while (true) {
    if (t_a->result_buffer[0][0] != '\0') {
      strcpy(final_result, get_best_suggestion(t_a->result_buffer));
      val = get_suggestion_value(t_a->result_buffer[0]);
    } else {
      strcpy(final_result, "No current suggestions");
    }
    printf("\r \033[1;36m> \033[0m%s \033[0;33m%s - %f\033[0m",
           t_a->input_buffer, t_a->result_buffer[0], val);
    fflush(stdout);
    // Wait for 100ms so the cursor doesnt go crazy
    usleep(100);
  }
}

// main loop, reads input, currently only recognized '.exit' as a valid command
int main(int argc, char *argv[]) {

  thread_args *t_a = malloc(sizeof(thread_args));
  t_a->input_buffer = malloc(sizeof(char) * MAX_INPUT_LENGTH);
  t_a->result_buffer = create_result_buffer();

  pthread_t t_id_search, t_id_input, t_id_gui;
  pthread_create(&t_id_search, NULL, search_loop, (void *)t_a);
  pthread_create(&t_id_input, NULL, input_loop, (void *)t_a);
  pthread_create(&t_id_gui, NULL, gui_loop, (void *)t_a);

  pthread_join(t_id_input, NULL);
  pthread_cancel(t_id_search);
  pthread_cancel(t_id_gui);

  return 0;
}
