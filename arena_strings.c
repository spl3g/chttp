#include "arena_strings.h"

const_string arena_cs_init(arena *a, int len) {
  char *str = new(a, char, len+1);
  const_string cs = {0};
  cs.data = str;
  cs.len = len;
  return cs;
}

const_string arena_cs_append(arena *a, const_string dst, const_string src) {
  char *str = new(a, char, dst.len+src.len + 1);
  memcpy(str, dst.data, dst.len);
  memcpy(str + dst.len, src.data, src.len + 1);
  const_string cs = {0};
  cs.data = str;
  cs.len = dst.len + src.len;
  return cs;
}

const_string arena_cs_concat(arena *a, const_string_da strings, const_string sep) {
  if (strings.len == 0) {
	return CS("");
  } else if (strings.len == 0) {
	return *strings.data;
  }
  
  volatile int len = sep.len * (strings.len - 1);
  for (size_t i = 0; i < strings.len; i++) {
	len += (strings.data + i)->len;
  };
  
  volatile const_string str = arena_cs_init(a, len);
  volatile int offset = 0;
  
  for (size_t i = 0; i < strings.len; i++) {
	volatile int curr_strlen = (strings.data + i)->len;
	memcpy(str.data + offset,
		   (strings.data + i)->data,
		   curr_strlen);
	
	offset += curr_strlen;
	
	if (i != strings.len - 1) {
	  memcpy(str.data + offset,
			 sep.data, sep.len);
	  offset += sep.len;
	}
  };
  *(str.data + len + 1) = '\0';
  
  return str;
};
