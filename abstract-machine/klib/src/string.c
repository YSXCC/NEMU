#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  size_t i = 0;
  while (*(s+i) != '\0') {
    i++;
  }
  return i;
}

char *strcpy(char *dst, const char *src) {
  size_t i = 0;
  while (src[i] != '\0') {
    dst[i] = src[i];
    i++;
  }
  dst[i] = '\0';
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  int i = 0;
  for ( ; i < n; ++i) {
    *(dst + i) = *(src + i);
  }
  return dst;
}

char *strcat(char *dst, const char *src) {
  int i = 0, j = 0;
  for(;*(dst+i) != '\0';++i){}
  for(;(*(dst+i) = *(src+j)) != '\0';++i,++j){}
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  if(s1==NULL){
    if(s2==NULL) return 0;
    else return -1;
  }
  if(s2==NULL){
    if(s1==NULL) return 0;
    else return 1;
  }
  int i;
  for (i = 0; s1[i] != '\0' && s2[i] != '\0'; ++i){
    if (s1[i] != s2[i])
      return (int)(s1[i]) - (int)(s2[i]);
  }
  return (int)(s1[i]) - (int)(s2[i]);
}

int strncmp(const char *s1, const char *s2, size_t n) {
  int i=0;
  for(;i<n;++i){
    if(*(s1+i) < *(s2+i)) return -1;
    else if(*(s1+i) > *(s2+i)) return 1;
  }
  return 0;
}

void *memset(void *s, int c, size_t n) {
  int i=0;
  for(;i<n;++i){
    *(char*)(s+i)=c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  panic("Not implemented");
}

void *memcpy(void *out, const void *in, size_t n) {
  char *char_out = (char *)out;
  const char *char_in = (const char *)in;
  for (int i = 0; i < n; ++i){
    char_out[i] = char_in[i];
  }

  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  int i=0;
  for(;i<n;++i){
    if(*(char*)(s1+i)<*(char*)(s2+i)) return -1;
    else if(*(char*)(s1+i)>*(char*)(s2+i)) return 1;
  }
  return 0;
}

#endif
