
typedef enum { TYPE_PDF, TYPE_TEXT, TYPE_NONE } FileType;

int open_file(char *file);
FileType get_file_type(char *file);
