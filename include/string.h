#ifndef STRING_H
#define STRING_H

#include "types.h"

void* memset(void* dst, int val, size_t count);
void* memcpy(void* dst, const void* src, size_t count);
size_t strlen(const char* str);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);
char* strcat(char* dst, const char* src);
void itoa(int32_t value, char* buf, int base);
void utoa(uint32_t value, char* buf, int base);

#endif
