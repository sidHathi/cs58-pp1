.SUFFIXES: .c


SRCS = convert.c album.c
OBJS = $(SRCS:.c=.o)
OUTPUT = convert album

CC = gcc
CFLAGS = 
LIBS = 

all: convert album

convert: convert.c
	gcc -o convert convert.c

album: album.c
	gcc -o album album.c

clean:
	rm -f $(OBJS) $(OUTPUT)

depend:
	makedepend -I/usr/local/include/g++ -- $(CFLAGS) -- $(SRCS) 

# DO NOT DELETE