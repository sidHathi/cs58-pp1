.SUFFIXES: .c

SRCS = album.c
OBJS = $(SRCS:.c=.o)
OUTPUT = album

all: album

album: album.c
	gcc -std=c99 -Wall -g -o album album.c

clean:
	rm -f $(OBJS) $(OUTPUT)

depend:
	makedepend -I/usr/local/include/g++ -- $(CFLAGS) -- $(SRCS) 

# DO NOT DELETE