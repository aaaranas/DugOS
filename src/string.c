/* =============================================================================
 * string.c -- DugOS kernel string utility implementation
 *
 * PURPOSE:
 *   Implements the minimal set of string and memory functions declared in
 *   string.h. These replace the standard library equivalents for use in a
 *   -nostdlib freestanding kernel where libc is not available.
 *
 * IMPLEMENTATION NOTES:
 *   All functions operate on bytes (char / uint8_t). None use compiler
 *   built-in optimizations that might reference undefined runtime helpers.
 *   Each is a straightforward loop implementation for correctness and
 *   readability, which is appropriate for a course OS.
 *
 * REFERENCES:
 *   C99 standard, Section 7.21 (string.h)
 * =============================================================================
 */

#include "string.h"

/* =============================================================================
 * kstrlen() -- count characters before '\0'
 * =============================================================================
 */
size_t kstrlen(const char *s)
{
    const char *p = s;
    while (*p) p++;       /* advance until null terminator */
    return (size_t)(p - s);
}

/* =============================================================================
 * kstrcmp() -- lexicographic comparison of two strings
 *
 * Returns:
 *   0  if strings are equal
 *  <0  if *a < *b at the first differing byte
 *  >0  if *a > *b at the first differing byte
 * =============================================================================
 */
int kstrcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* =============================================================================
 * kstrncmp() -- compare at most n bytes
 * =============================================================================
 */
int kstrncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* =============================================================================
 * kstrcpy() -- copy string including null terminator
 * =============================================================================
 */
char *kstrcpy(char *dst, const char *src)
{
    char *p = dst;
    while ((*p++ = *src++));   /* copy each byte including '\0' */
    return dst;
}

/* =============================================================================
 * kstrncpy() -- copy at most n bytes, zero-padding if src is shorter
 * =============================================================================
 */
char *kstrncpy(char *dst, const char *src, size_t n)
{
    char *p = dst;
    while (n && (*p = *src)) { p++; src++; n--; }  /* copy up to n chars */
    while (n--) *p++ = '\0';                        /* zero-pad remainder */
    return dst;
}

/* =============================================================================
 * kstrcat() -- append src to end of dst
 * =============================================================================
 */
char *kstrcat(char *dst, const char *src)
{
    char *p = dst;
    while (*p) p++;            /* advance to the end of dst */
    while ((*p++ = *src++));   /* append src including '\0' */
    return dst;
}

/* =============================================================================
 * kstrchr() -- find first occurrence of character c in string s
 *
 * Returns:
 *   Pointer to the first occurrence of c in s, or NULL if not found.
 *   Searching for '\0' returns a pointer to the null terminator.
 * =============================================================================
 */
char *kstrchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    /* Handle the case where c == '\0': return pointer to null terminator. */
    return (c == '\0') ? (char *)s : (char *)0;
}

/* =============================================================================
 * kmemset() -- fill memory with a byte value
 * =============================================================================
 */
void *kmemset(void *dst, int c, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

/* =============================================================================
 * kmemcpy() -- copy n bytes from src to dst (non-overlapping regions)
 * =============================================================================
 */
void *kmemcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
