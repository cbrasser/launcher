#include <string.h>
#include <stdlib.h>

#include "../headers/file_handlers.h"

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

