#include "../headers/constants.h"
#include "../headers/suggestion_logic.h"

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

