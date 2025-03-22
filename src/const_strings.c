#include <chttp/const_strings.h>

const_string cs_from_parts(const char* data, int len) {
  const_string str;
  str.data = data;
  str.len = len;

  return str;
}

const_string cs_from_cstr(const char* cstr) {
  return cs_from_parts(cstr, strlen(cstr));
}

const_string cs_slice(const_string src, int from, int to) {
  if (from > src.len) {
	return src;
  }

  int new_from;
  if (from <= 0) {
	new_from = 1;
  } 
  new_from -= 1;

  int new_len;
  if (to > src.len) {
	new_len = src.len;
  } else {
	new_len = to;
  }

  return cs_from_parts(src.data + new_from, new_len);
}

const_string cs_chop_delim(const_string *str, char delim) {
  int n;
  for (n = 0; n < str->len && str->data[n] != delim; n++) {}

  const_string result = cs_from_parts(str->data, n);

  if (n < str->len) {
	str->len -= n + 1;
	str->data += n + 1;
  } else {
	str->len -= n;
	str->data += n;
  }

  return result;
}

bool cs_try_chop_delim(const_string *str, char delim, const_string *dst) {
  const_string str_copy = *str;
  const_string tmp = cs_chop_delim(&str_copy, delim);
  if (str_copy.len == 0) {
	return false;
  }
  *str = str_copy;
  *dst = tmp;
  return true;
}

int cs_find_delim(const_string str, char delim) {
  int n;
  for (n = 0; n < str.len && str.data[n] != delim; n++) {}

  if (n < str.len) {
	return n;
  }
  return -1;
}

bool cs_eq(const_string str1, const_string str2) {
  if (str1.len != str2.len) {
	return false;
  }
  return memcmp(str1.data, str2.data, str1.len) == 0;
}

void cs_print(char *format, const_string str) {
  char buf[str.len + 1];
  memcpy(buf, str.data, str.len);
  buf[str.len] = '\0';
  printf(format, buf);
}
