#include "string.h"

void* memset(void* dst, int val, size_t count) {
    uint8_t* p = (uint8_t*)dst;
    uint8_t v = (uint8_t)val;
    while (count--) {
        *p++ = v;
    }
    return dst;
}

void* memcpy(void* dst, const void* src, size_t count) {
    uint32_t* d32 = (uint32_t*)dst;
    const uint32_t* s32 = (const uint32_t*)src;
    while (count >= 4) {
        *d32++ = *s32++;
        count -= 4;
    }
    uint8_t* d8 = (uint8_t*)d32;
    const uint8_t* s8 = (const uint8_t*)s32;
    while (count--) {
        *d8++ = *s8++;
    }
    return dst;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) {
        a++;
        b++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

char* strcpy(char* dst, const char* src) {
    char* orig = dst;
    while ((*dst++ = *src++));
    return orig;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* orig = dst;
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return orig;
}

char* strcat(char* dst, const char* src) {
    char* orig = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return orig;
}

void utoa(uint32_t value, char* buf, int base) {
    const char* digits = "0123456789ABCDEF";
    char tmp[36];
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (value > 0) {
        tmp[i++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

void itoa(int32_t value, char* buf, int base) {
    if (value < 0 && base == 10) {
        buf[0] = '-';
        utoa((uint32_t)(-value), buf + 1, base);
    } else {
        utoa((uint32_t)value, buf, base);
    }
}
