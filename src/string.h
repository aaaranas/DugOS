/* =============================================================================
 * string.h -- DugOS kernel string utility interface
 *
 * PURPOSE:
 *   Provides minimal string and memory manipulation functions for use in the
 *   DugOS kernel. Because the kernel is built with -nostdlib, the standard
 *   C library's <string.h> is not available. This module re-implements only
 *   the subset needed by the shell, file system, and directory subsystems.
 *
 * NAMING:
 *   All functions use the 'k' prefix (e.g. kstrlen, kstrcmp) to make it
 *   obvious they are kernel-internal helpers and to avoid any name clash with
 *   compiler built-ins that might reference the missing libc stubs.
 *
 * NOTE ON SAFETY:
 *   These functions do NOT check for NULL pointers. Callers are responsible
 *   for passing valid, non-NULL string pointers. In a ring-0 kernel there is
 *   no fault handler that can recover gracefully from a NULL dereference.
 * =============================================================================
 */

#ifndef DUGOS_STRING_H
#define DUGOS_STRING_H

#include <stddef.h>   /* size_t */
#include <stdint.h>

/* -- Return the number of characters before the null terminator. */
size_t kstrlen(const char *s);

/* -- Compare two null-terminated strings.
 *    Returns 0 if equal, <0 if a<b, >0 if a>b (same semantics as strcmp). */
int kstrcmp(const char *a, const char *b);

/* -- Compare at most n bytes of two strings. */
int kstrncmp(const char *a, const char *b, size_t n);

/* -- Copy src into dst (including the null terminator). Returns dst. */
char *kstrcpy(char *dst, const char *src);

/* -- Copy at most n bytes of src into dst. Pads with '\0' if src is shorter
 *    than n. Does NOT guarantee null-termination if src is longer than n-1:
 *    callers should ensure dst has space for n+1 bytes or force-terminate. */
char *kstrncpy(char *dst, const char *src, size_t n);

/* -- Append src to the end of dst. dst must have enough space. Returns dst. */
char *kstrcat(char *dst, const char *src);

/* -- Find the first occurrence of character c in s. Returns pointer to it
 *    or NULL if not found (searches including the null terminator for c==0). */
char *kstrchr(const char *s, int c);

/* -- Fill n bytes starting at dst with the byte value c. Returns dst. */
void *kmemset(void *dst, int c, size_t n);

/* -- Copy n bytes from src to dst (regions must not overlap). Returns dst. */
void *kmemcpy(void *dst, const void *src, size_t n);

/* -- Convert character to lowercase if it is an uppercase ASCII letter. */
static inline int kto_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

/* -- Return 1 if c is a printable ASCII character (0x20-0x7E). */
static inline int kis_print(int c)
{
    return (c >= 0x20 && c <= 0x7E);
}

#endif /* DUGOS_STRING_H */
