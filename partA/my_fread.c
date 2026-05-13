#include "my_fread.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

my_file_t *my_fopen(const char *path, size_t buffer_size)
{
    my_file_t *stream;

    if (buffer_size == 0) {
        errno = EINVAL;
        return NULL;
    }

    stream = calloc(1, sizeof(*stream));
    if (stream == NULL) {
        return NULL;
    }

    stream->buffer = malloc(buffer_size);
    if (stream->buffer == NULL) {
        free(stream);
        return NULL;
    }

    stream->fd = open(path, O_RDONLY);
    if (stream->fd < 0) {
        free(stream->buffer);
        free(stream);
        return NULL;
    }

    stream->capacity = buffer_size;
    return stream;
}

static int refill(my_file_t *stream)
{
    ssize_t nread;

    while (1) {
        nread = read(stream->fd, stream->buffer, stream->capacity);
        if (nread < 0 && errno == EINTR) {
            continue;
        }
        break;
    }

    if (nread < 0) {
        stream->error = errno;
        return -1;
    }
    if (nread == 0) {
        stream->eof = 1;
        return 0;
    }

    stream->pos = 0;
    stream->len = (size_t)nread;
    return 1;
}

ssize_t my_fread(void *ptr, size_t size, size_t nmemb, my_file_t *stream)
{
    unsigned char *out = ptr;
    size_t total;
    size_t copied = 0;

    if (stream == NULL || ptr == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    if (nmemb > SIZE_MAX / size) {
        errno = EOVERFLOW;
        return -1;
    }

    total = size * nmemb;

    while (copied < total) {
        size_t available;
        size_t want;

        if (stream->pos == stream->len) {
            int status;

            if (stream->eof) {
                break;
            }
            status = refill(stream);
            if (status < 0) {
                return copied > 0 ? (ssize_t)(copied / size) : -1;
            }
            if (status == 0) {
                break;
            }
        }

        available = stream->len - stream->pos;
        want = total - copied;
        if (want > available) {
            want = available;
        }

        memcpy(out + copied, stream->buffer + stream->pos, want);
        stream->pos += want;
        copied += want;
    }

    return (ssize_t)(copied / size);
}

int my_fclose(my_file_t *stream)
{
    int ret = 0;

    if (stream == NULL) {
        return 0;
    }

    if (stream->fd >= 0) {
        ret = close(stream->fd);
    }
    free(stream->buffer);
    free(stream);
    return ret;
}

int my_ferror(my_file_t *stream)
{
    if (stream == NULL) {
        return EINVAL;
    }
    return stream->error;
}
