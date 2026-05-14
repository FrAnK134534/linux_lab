CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -pthread
CPPFLAGS ?=
LDFLAGS ?= -pthread

COMMON_OBJS := src/utils.o
TARGETS := poll_server threadpool_server epoll_server io_uring_server client_bench

HAVE_LIBURING ?= $(shell pkg-config --exists liburing 2>/dev/null && echo 1)
ifeq ($(HAVE_LIBURING),1)
CPPFLAGS += -DHAVE_LIBURING $(shell pkg-config --cflags liburing 2>/dev/null)
IO_URING_LIBS := $(shell pkg-config --libs liburing 2>/dev/null)
ifeq ($(IO_URING_LIBS),)
IO_URING_LIBS := -luring
endif
else
IO_URING_LIBS :=
endif

.PHONY: all clean

all: $(TARGETS)

poll_server: src/poll_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

threadpool_server: src/threadpool_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

epoll_server: src/epoll_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

io_uring_server: src/io_uring_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(IO_URING_LIBS)

client_bench: src/client_bench.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c src/common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) src/*.o *.csv
