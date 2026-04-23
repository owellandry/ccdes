CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lcurl

ifeq ($(OS),Windows_NT)
    TARGET  = ccdes.exe
    LDFLAGS += -lws2_32
else
    TARGET  = ccdes
endif

SRCS = main.c download.c parser.c reconstruct.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c ccdes.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	./$(TARGET) --test

.PHONY: all clean test
