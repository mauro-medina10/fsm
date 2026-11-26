CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -I../include
LDFLAGS := -fsanitize=address,undefined

SRCS := test_fsm.c ../fsm.c ../ring_buff.c
TARGET := test_fsm

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) ../include/fsm.h ../include/ring_buff.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)
	python3 test_mermaid.py -v

clean:
	rm -f $(TARGET)
