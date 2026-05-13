#ifndef MY_FREAD_H
#define MY_FREAD_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    int fd;
    unsigned char *buffer;
    size_t capacity;
    size_t pos;
    size_t len;
    int eof;
    int error;
} my_file_t;

my_file_t *my_fopen(const char *path, size_t buffer_size);
ssize_t my_fread(void *ptr, size_t size, size_t nmemb, my_file_t *stream);
int my_fclose(my_file_t *stream);
int my_ferror(my_file_t *stream);

#endif
