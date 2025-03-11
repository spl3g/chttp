main: main.c lib
	cc -Wall -Wextra -ggdb -o main main.c lib/arena_strings.c lib/http.c lib/arena.c lib/const_strings.c
