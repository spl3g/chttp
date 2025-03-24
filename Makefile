CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -fPIC -Iinclude/

LIBNAME = libchttp
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)

all: static

static: $(OBJECTS)
	$(AR) rcs $(LIBNAME).a $(OBJECTS)

shared: $(OBJECTS)
	$(CC) -shared -o $(LIBNAME).so $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	install -d /usr/local/include/yourlib/
	install include/yourlib/*.h /usr/local/include/yourlib/
	install $(LIBNAME) /usr/local/lib/

clean:
	rm -f src/*.o *.a *.so
