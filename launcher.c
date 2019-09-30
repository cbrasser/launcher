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

// Constant definition, TODO: put in separate config file
#define MAX_INPUT_LENGTH 100
#define MAX_NUM_RESULTS 100
#define MAX_RESULT_LENGTH 1000
#define MAX_PATH_DEPTH 1000
#define MAX_NUM_SUBDIRS 1000
#define BASE_PATH "/home/clados"
#define DELIMITER '/'
#define INCLUDE_HIDDEN 0
#define OPEN_PDF "mupdf"
#define EXIT_KEY ' '

pthread_mutex_t input_lock;
pthread_mutex_t suggestion_lock;
/*
 *------------------I/O related stuff------------------
 */

typedef enum { ERROR_INPUT_TOO_LONG } Errors;
typedef enum { RESULT_FOUND, RESULT_NOT_FOUND } Result;
typedef enum { TYPE_PDF, TYPE_TEXT, TYPE_NONE } FileType;

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

int search_fs(char ***dir_list, char ***list, char *file_name, char *directory,
              int *location, int *dir_location) {
  DIR *opened_dir;
  struct dirent *directory_structure;

  // printf("Entered search in dir: %s\n", directory);
  opened_dir = opendir(directory);
  if (opened_dir == NULL) {
    printf("could not open dir: %s\n", directory);
    return -1;
  }
  int sub_dir_count = 0;
  strcat(directory, "/");

  // While we get stuff from the readdir stream
  while (1) {
    // We want to simulate a BFS: We want to check the files in the current
    // folder first, and then go into the subfolders!
    directory_structure = readdir(opened_dir);
    // Stream is done -> continue onto handling diretories
    if (!directory_structure) {
      closedir(opened_dir);
      break;
    }
    // Current item from stream is dir, push it onto stack if its not hidden,
    // back or current
    if (directory_structure->d_type == DT_DIR) {
      if (!strcmp(directory_structure->d_name, "..") ||
          !strcmp(directory_structure->d_name, ".") ||
          (!strncmp(directory_structure->d_name, ".", 1) && !INCLUDE_HIDDEN))
        continue;
      // printf("Pushing dir %s onto stack\n", directory_structure->d_name);
      (*dir_list)[*dir_location] =
          (char *)malloc(sizeof(char) * MAX_PATH_DEPTH);
      sprintf((*dir_list)[*dir_location], "%s%s", directory,
              directory_structure->d_name);
      (*dir_location)++;
      sub_dir_count++;
      continue;
    }

    // found a file with correct name, append it to list of found files
    if (!strcmp(directory_structure->d_name, file_name)) {
      // Append file/dir to list and go on
      (*list)[*location] = (char *)malloc(sizeof(char) * MAX_RESULT_LENGTH);
      sprintf((*list)[*location], "%s%s", directory,
              directory_structure->d_name);
      (*location)++;
    }
  }
  // free(directory_structure);

    int initial_dir_location = *dir_location;
    while (*dir_location > initial_dir_location - sub_dir_count) {
      search_fs(dir_list, list, file_name, (*dir_list)[(*dir_location)-1],
                location, dir_location);
      (*dir_location)--;
      free((*dir_list)[*dir_location]);
    }

  return 0;
}

int find_file(char *input_buffer, char **result_buffer) {
  char **found_list = (char **)malloc(sizeof(char *) * MAX_NUM_RESULTS);
  if (!found_list) {
    printf("could not get result list\n");
    return 0;
  }
  char **dir_list = (char **)malloc(sizeof(char *) * MAX_NUM_SUBDIRS);
  if (!dir_list) {
    free(found_list);
    return 0;
  }
  int location_counter = 0;
  int dir_location_counter = 0;
  char *ini_dir = (char *)malloc(sizeof(char) * 500);
  if (!ini_dir) {
    free(found_list);
    free(dir_list);
    return 0;
  }

  strcpy(ini_dir, BASE_PATH);
  search_fs(&dir_list, &found_list, input_buffer, ini_dir, &location_counter,
            &dir_location_counter);
  if (location_counter > 0) {
    while (--location_counter >= 0) {
      strcpy(result_buffer[location_counter], found_list[location_counter]);
      free(found_list[location_counter]);
    }
  } else {
    // printf("Failed to find the file!\n");
  }

  free(ini_dir);
  free(found_list);
  free(dir_list);

  return 1;
}

// Estimate the "value" of a possible search result
// to provide better suggestions, currently just path length
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
  depth = 1 / (depth / MAX_PATH_DEPTH);
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

FileType get_file_type(char *file) {
  int last_index = -1;
  char extension[10];
  for (int i = strlen(file) - 1; i >= 0; i--) {
    if (file[i] == '.') {
      last_index = i;
    }
  }
  if (last_index == -1) {
    return TYPE_NONE;
  } else {
    memcpy(extension, &file[last_index], strlen(file) - last_index);
  }
  if (strcmp(extension, ".pdf") == 0) {
    return TYPE_PDF;
  } else if (strcmp(extension, ".txt") == 0) {
    return TYPE_TEXT;
  } else {
    return TYPE_NONE;
  }
}

int open_file(char *file) {
  char command[100];
  FileType type = get_file_type(file);
  switch (type) {
  case TYPE_PDF:
    strcpy(command, "mupdf ");
    break;
  case TYPE_TEXT:
    strcpy(command, "st nvim ");
    break;
  case TYPE_NONE:
    // For now we try to open everythin without a file extension as a directory
    strcpy(command, "st fff ");
  }
  strcat(command, file);
  // Release command to continue
  strcat(command, " &");
  system(command);
  return 1;
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
    } else if ((c >= 97 && c <= 122) || c == 46 || c == 95 ||
               (c >= 68 && c <= 90) || (c >= 48 && c <= 57)) { // normal char
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

// main loop, reads input, currently only recognized '.exit' as a valid command
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
