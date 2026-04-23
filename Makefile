CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lcurl

ifeq ($(OS),Windows_NT)
    TARGET  = ccdes.exe
    LDFLAGS += -lws2_32
    RM = del /Q
else
    TARGET  = ccdes
    RM = rm -f
endif

SRCS = main.c download.c parser.c reconstruct.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c ccdes.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET) ccdes.exe

test: $(TARGET)
	./$(TARGET) --test

.PHONY: all clean test
