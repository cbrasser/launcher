#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../headers/constants.h"
#include "../headers/search.h"

int search_fs(char ***dir_list, char ***list, char *file_name, char *directory,
              int *location, int *dir_location, int depth) {
  DIR *opened_dir;
  struct dirent *directory_structure;

  opened_dir = opendir(directory);
  if (opened_dir == NULL) {
    // printf("could not open dir: %s\n", directory);
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
    if (directory_structure->d_type == DT_DIR && depth < MAX_PATH_DEPTH) {
      if (!strcmp(directory_structure->d_name, "..") ||
          !strcmp(directory_structure->d_name, ".") ||
          (!strncmp(directory_structure->d_name, ".", 1) && !INCLUDE_HIDDEN))
        continue;
      (*dir_list)[*dir_location] =
          (char *)malloc(sizeof(char) * MAX_RESULT_LENGTH);
      sprintf((*dir_list)[*dir_location], "%s%s", directory,
              directory_structure->d_name);
      (*dir_location)++;
      sub_dir_count++;
      //continue;
    }

    // found a file with correct name, append it to list of found files
    if (!strncmp(directory_structure->d_name, file_name, strlen(file_name)) && *location < MAX_NUM_RESULTS) {
       // printf("found partial match: %s\n", directory_structure->d_name);
      // Append file/dir to list and go on
      (*list)[*location] = (char *)malloc(sizeof(char) * MAX_RESULT_LENGTH);
      sprintf((*list)[*location], "%s%s", directory,
              directory_structure->d_name);
      (*location)++;
    }
  }

    int initial_dir_location = *dir_location;
    while (*dir_location > initial_dir_location - sub_dir_count) {
      search_fs(dir_list, list, file_name, (*dir_list)[(*dir_location)-1],
                location, dir_location, depth+1);
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
            &dir_location_counter, 0);
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

