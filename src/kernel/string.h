/* DEBIL OS - tiny freestanding string library */
#ifndef DEBIL_STRING_H
#define DEBIL_STRING_H

#include "types.h"

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int starts_with(const char *s, const char *prefix);
int ends_with(const char *s, const char *suffix);

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

int atoi(const char *s);
void utoa(uint32_t value, char *buf, int base);
void itoa(int value, char *buf, int base);
int tolower_c(int c);
int toupper_c(int c);
void str_tolower(char *s);
void str_trim(char *s);

#endif
