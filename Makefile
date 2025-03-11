main: main.c const_strings.h arena.h arena_strings.h arena_strings.c
	cc -Wall -Wextra -ggdb -o main main.c arena_strings.c
