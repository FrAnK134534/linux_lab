CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS ?=
LDFLAGS ?=

TARGET := io_bench
OBJS := main.o my_fread.o

.PHONY: all clean run-small

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run-small: $(TARGET)
	./$(TARGET) write test_write.bin 1048576
	./$(TARGET) all_read test_write.bin

clean:
	rm -f $(TARGET) $(OBJS) test_write.bin out.bin *.csv
