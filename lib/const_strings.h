#ifndef CONST_STRINGS_H_
#define CONST_STRINGS_H_

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
  const char* data;
  int len;
} const_string;

const_string cs_from_parts(const char* data, int len);
const_string cs_from_cstr(const char* cstr);
const_string cs_slice(const_string src, int from, int to);
const_string cs_chop_delim(const_string *src, char delim);
bool cs_try_chop_delim(const_string *str, char delim, const_string *dst);
int cs_find_delim(const_string str, char delim);
bool cs_eq(const_string str1, const_string str2);
void cs_print(char *format, const_string str);

#define CS(cstr) cs_from_parts(cstr, sizeof(cstr) - 1)
#define CS_STATIC(cstr) {.data = cstr, .len = sizeof(cstr) - 1}

#define CS_FMT "%.*s"
#define CS_ARG(cs) (int) (cs).len, (cs).data

#endif // STRINGS_H_
