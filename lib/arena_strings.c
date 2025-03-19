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
  
  int len = sep.len * (strings.len - 1);
  for (size_t i = 0; i < strings.len; i++) {
	len += (strings.data + i)->len;
  };
  
  char* str = arena_alloc(a, len);
  int offset = 0;
  
  for (size_t i = 0; i < strings.len; i++) {
	int curr_strlen = (strings.data + i)->len;
	memcpy(str + offset,
		   (strings.data + i)->data,
		   curr_strlen);
	
	offset += curr_strlen;
	
	if (i != strings.len - 1) {
	  memcpy(str + offset,
			 sep.data, sep.len);
	  offset += sep.len;
	}
  };
  *(str + len + 1) = '\0';
  
  return (const_string){str, len};
};
