CFLAGS = -std=gnu99 -g -Wall
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

all: $(OBJS)

%.o: %.c
	gcc -c $(CFLAGS) $< -o $@

clean:
	rm -rf -- *.o
