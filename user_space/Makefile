LINKFLAGS=-lpthread
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

host: $(OBJS)
	gcc $(LINKFLAGS) -o host $(OBJS)

%.o: %.c
	gcc -c -o $@ $<

clean:
	rm $(OBJS)
