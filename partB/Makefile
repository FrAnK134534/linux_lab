CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -pthread
CPPFLAGS ?=
LDFLAGS ?= -pthread

COMMON_OBJS := src/utils.o
TARGETS := poll_server threadpool_server epoll_server client_bench

.PHONY: all clean

all: $(TARGETS)

poll_server: src/poll_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

threadpool_server: src/threadpool_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

epoll_server: src/epoll_server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

client_bench: src/client_bench.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

src/%.o: src/%.c src/common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) src/*.o *.csv
