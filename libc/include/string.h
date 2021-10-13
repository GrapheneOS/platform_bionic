/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _STRING_H
#define _STRING_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <xlocale.h>

#include <bits/strcasecmp.h>

__BEGIN_DECLS

#if defined(__USE_BSD) || defined(__USE_GNU)
#include <strings.h>
#endif

void* _Nullable memccpy(void* _Nonnull __dst, const void* _Nonnull __src, int __stop_char, size_t __n);
void* _Nullable memchr(const void* _Nonnull __s, int __ch, size_t __n) __attribute_pure__;
#if defined(__cplusplus)
extern "C++" void* _Nullable memrchr(void* _Nonnull __s, int __ch, size_t __n) __RENAME(memrchr) __attribute_pure__;
extern "C++" const void* _Nullable memrchr(const void* _Nonnull __s, int __ch, size_t __n) __RENAME(memrchr) __attribute_pure__;
#else
void* _Nullable memrchr(const void* _Nonnull __s, int __ch, size_t __n) __attribute_pure__;
#endif
int memcmp(const void* _Nonnull __lhs, const void* _Nonnull __rhs, size_t __n) __attribute_pure__;
void* _Nonnull memcpy(void* _Nonnull, const void* _Nonnull, size_t);

#if defined(__USE_GNU) && __BIONIC_AVAILABILITY_GUARD(23)
void* _Nonnull mempcpy(void* _Nonnull __dst, const void* _Nonnull __src, size_t __n) __INTRODUCED_IN(23);
#endif

void* _Nonnull memmove(void* _Nonnull __dst, const void* _Nonnull __src, size_t __n);

/**
 * [memset(3)](https://man7.org/linux/man-pages/man3/memset.3.html) writes the
 * bottom 8 bits of the given int to the next `n` bytes of `dst`.
 *
 * Returns `dst`.
 */
void* _Nonnull memset(void* _Nonnull __dst, int __ch, size_t __n);

void* _Nonnull explicit_bzero(void* _Nonnull s, size_t n);

/**
 * [memset_explicit(3)](https://man7.org/linux/man-pages/man3/memset_explicit.3.html)
 * writes the bottom 8 bits of the given int to the next `n` bytes of `dst`,
 * but won't be optimized out by the compiler.
 *
 * Returns `dst`.
 *
 * Available from API level 34, or with __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__.
 */
#if __ANDROID_API__ >= 34
void* _Nonnull memset_explicit(void* _Nonnull __dst, int __ch, size_t __n) __INTRODUCED_IN(34);
#elif defined(__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__)
#define __BIONIC_MEMSET_EXPLICIT_INLINE static __inline
#include <bits/memset_explicit_impl.h>
#undef __BIONIC_MEMSET_EXPLICIT_INLINE
#endif

void* _Nullable memmem(const void* _Nonnull __haystack, size_t __haystack_size, const void* _Nonnull __needle, size_t __needle_size) __attribute_pure__;

char* _Nullable strchr(const char* _Nonnull __s, int __ch) __attribute_pure__;
char* _Nullable __strchr_chk(const char* _Nonnull __s, int __ch, size_t __n);

#if defined(__USE_GNU) && __BIONIC_AVAILABILITY_GUARD(24)
#if defined(__cplusplus)
extern "C++" char* _Nonnull strchrnul(char* _Nonnull __s, int __ch) __RENAME(strchrnul) __attribute_pure__ __INTRODUCED_IN(24);
extern "C++" const char* _Nonnull strchrnul(const char* _Nonnull __s, int __ch) __RENAME(strchrnul) __attribute_pure__ __INTRODUCED_IN(24);
#else
char* _Nonnull strchrnul(const char* _Nonnull __s, int __ch) __attribute_pure__ __INTRODUCED_IN(24);
#endif
#endif

char* _Nullable strrchr(const char* _Nonnull __s, int __ch) __attribute_pure__;
char* _Nullable __strrchr_chk(const char* _Nonnull __s, int __ch, size_t __n);

size_t strlen(const char* _Nonnull __s) __attribute_pure__;
size_t __strlen_chk(const char* _Nonnull __s, size_t __n);

int strcmp(const char* _Nonnull __lhs, const char* _Nonnull __rhs) __attribute_pure__;
char* _Nonnull stpcpy(char* _Nonnull __dst, const char* _Nonnull __src);
char* _Nonnull strcpy(char* _Nonnull __dst, const char* _Nonnull __src);
char* _Nonnull strcat(char* _Nonnull __dst, const char* _Nonnull __src);
char* _Nullable strdup(const char* _Nonnull __s);

char* _Nullable strstr(const char* _Nonnull __haystack, const char* _Nonnull __needle) __attribute_pure__;
char* _Nullable strcasestr(const char* _Nonnull __haystack, const char* _Nonnull __needle) __attribute_pure__;

/**
 * [strtok(3)](https://man7.org/linux/man-pages/man3/strtok.3.html)
 * extracts non-empty tokens from strings.
 *
 * Code on Android should use strtok_r() instead,
 * since strtok() isn't thread-safe.
 *
 * See strsep() if you want empty tokens returned too.
 */
char* _Nullable strtok(char* _Nullable __s, const char* _Nonnull __delimiter)
    __attribute__((__deprecated__("strtok() is not thread-safe; use strtok_r() instead")));

/**
 * [strtok_r(3)](https://man7.org/linux/man-pages/man3/strtok_r.3.html)
 * extracts non-empty tokens from strings.
 *
 * See strsep() if you want empty tokens returned too.
 */
char* _Nullable strtok_r(char* _Nullable __s, const char* _Nonnull __delimiter, char* _Nonnull * _Nonnull __pos_ptr);

/**
 * [strerror(3)](https://man7.org/linux/man-pages/man3/strerror.3.html)
 * returns a string describing the given errno value.
 * `strerror(EINVAL)` would return "Invalid argument", for example.
 *
 * On Android, unknown errno values return a string such as "Unknown error 666".
 * These unknown errno value strings live in thread-local storage, and are valid
 * until the next call of strerror() on the same thread.
 *
 * Returns a pointer to a string.
 */
char* _Nonnull strerror(int __errno_value);

/**
 * Equivalent to strerror() on Android where only C/POSIX locales are available.
 */
char* _Nonnull strerror_l(int __errno_value, locale_t _Nonnull __l) __RENAME(strerror);

/**
 * [strerror_r(3)](https://man7.org/linux/man-pages/man3/strerror_r.3.html)
 * writes a string describing the given errno value into the given buffer.
 *
 * There are two variants of this function, POSIX and GNU.
 * The GNU variant returns a pointer to the buffer.
 * The POSIX variant returns 0 on success or an errno value on failure.
 *
 * The GNU variant is available since API level 23 if `_GNU_SOURCE` is defined.
 * The POSIX variant is available otherwise.
 */
#if defined(__USE_GNU) && __ANDROID_API__ >= 23
char* _Nonnull strerror_r(int __errno_value, char* _Nullable __buf, size_t __n) __RENAME(__gnu_strerror_r) __INTRODUCED_IN(23);
#else /* POSIX */
int strerror_r(int __errno_value, char* _Nonnull __buf, size_t __n);
#endif

/**
 * [strerrorname_np(3)](https://man7.org/linux/man-pages/man3/strerrorname_np.3.html)
 * returns the name of the errno constant corresponding to its argument.
 * `strerrorname_np(38)` would return "ENOSYS", because `ENOSYS` is errno 38. This
 * is mostly useful for error reporting in cases where a string like "ENOSYS" is
 * more readable than a string like "Function not implemented", which would be
 * returned by strerror().
 *
 * Returns a pointer to a string, or null for unknown errno values.
 *
 * Available since API level 35 when compiling with `_GNU_SOURCE`.
 */
#if defined(__USE_GNU) && __BIONIC_AVAILABILITY_GUARD(35)
const char* _Nullable strerrorname_np(int __errno_value) __INTRODUCED_IN(35);
#endif

/**
 * [strerrordesc_np(3)](https://man7.org/linux/man-pages/man3/strerrordesc_np.3.html)
 * is like strerror() but without localization. Since Android's strerror()
 * does not localize, this is the same as strerror() on Android.
 *
 * Returns a pointer to a string.
 *
 * Available when compiling with `_GNU_SOURCE`.
 */
#if defined(__USE_GNU)
const char* _Nonnull strerrordesc_np(int __errno_value) __RENAME(strerror);
#endif

size_t strnlen(const char* _Nonnull __s, size_t __n) __attribute_pure__;
char* _Nonnull strncat(char* _Nonnull __dst, const char* _Nonnull __src, size_t __n);
char* _Nullable strndup(const char* _Nonnull __s, size_t __n);
int strncmp(const char* _Nonnull __lhs, const char* _Nonnull __rhs, size_t __n) __attribute_pure__;
char* _Nonnull stpncpy(char* _Nonnull __dst, const char* _Nonnull __src, size_t __n);
char* _Nonnull strncpy(char* _Nonnull __dst, const char* _Nonnull __src, size_t __n);

size_t strlcat(char* _Nonnull __dst, const char* _Nonnull __src, size_t __n);
size_t strlcpy(char* _Nonnull __dst, const char* _Nonnull __src, size_t __n);

/**
 * [strcspn(3)](https://man7.org/linux/man-pages/man3/strcspn.3.html)
 * returns the length of the prefix containing only characters _not_ in
 * the reject set.
 */
size_t strcspn(const char* _Nonnull __s, const char* _Nonnull __reject) __attribute_pure__;

/**
 * [strpbrk(3)](https://man7.org/linux/man-pages/man3/strpbrk.3.html)
 * returns a pointer to the first character in the string that's
 * in the accept set, or null.
 *
 * See strspn() if you want an index instead.
 */
char* _Nullable strpbrk(const char* _Nonnull __s, const char* _Nonnull __accept) __attribute_pure__;

/**
 * [strsep(3)](https://man7.org/linux/man-pages/man3/strsep.3.html)
 * extracts tokens (including empty ones) from strings.
 *
 * See strtok_r() if you don't want empty tokens.
 */
char* _Nullable strsep(char* _Nullable * _Nonnull __s_ptr, const char* _Nonnull __delimiter);

/**
 * [strspn(3)](https://man7.org/linux/man-pages/man3/strspn.3.html)
 * returns the length of the prefix containing only characters in
 * the accept set.
 *
 * See strpbrk() if you want a pointer instead.
 */
size_t strspn(const char* _Nonnull __s, const char* _Nonnull __accept);

/**
 * [strsignal(3)](https://man7.org/linux/man-pages/man3/strsignal.3.html)
 * converts the integer corresponding to SIGSEGV (say) into a string
 * like "Segmentation violation".
 *
 * Use sig2str() instead to convert the integer corresponding to SIGSEGV (say)
 * into a string like "SEGV".
 *
 * Returns a pointer to a string. For invalid signals, the string is in TLS.
 */
char* _Nonnull strsignal(int __signal);

/** Equivalent to strcmp() on Android. */
int strcoll(const char* _Nonnull __lhs, const char* _Nonnull __rhs) __attribute_pure__;
/** Equivalent to strcmp() on Android. */
int strcoll_l(const char* _Nonnull __lhs, const char* _Nonnull __rhs, locale_t _Nonnull __l) __attribute_pure__;

/** Equivalent to strlcpy() on Android. */
size_t strxfrm(char* __BIONIC_COMPLICATED_NULLNESS __dst, const char* _Nonnull __src, size_t __n);
/** Equivalent to strlcpy() on Android. */
size_t strxfrm_l(char* __BIONIC_COMPLICATED_NULLNESS __dst, const char* _Nonnull __src, size_t __n, locale_t _Nonnull __l);

/*
 * glibc has a basename in <string.h> that's different to the POSIX one in <libgen.h>.
 * It doesn't modify its argument, and in C++ it's const-correct.
 */
#if defined(__USE_GNU) && __BIONIC_AVAILABILITY_GUARD(23) && !defined(basename)
#if defined(__cplusplus)
extern "C++" char* _Nonnull basename(char* _Nullable __path) __RENAME(__gnu_basename) __INTRODUCED_IN(23);
extern "C++" const char* _Nonnull basename(const char* _Nonnull __path) __RENAME(__gnu_basename) __INTRODUCED_IN(23);
#else
char* _Nonnull basename(const char* _Nonnull __path) __RENAME(__gnu_basename) __INTRODUCED_IN(23);
#endif
#endif

#if defined(__BIONIC_INCLUDE_FORTIFY_HEADERS)
#include <bits/fortify/string.h>
#endif

/* Const-correct overloads. Placed after FORTIFY so we call those functions, if possible. */
#if defined(__cplusplus)
/* libcxx tries to provide these. Suppress that, since libcxx's impl doesn't respect FORTIFY. */
#define __CORRECT_ISO_CPP_STRING_H_PROTO
/* Used to make these preferable over regular <string.h> signatures for overload resolution. */
#define __prefer_this_overload __enable_if(true, "")
extern "C++" {
inline __always_inline
void* _Nullable __bionic_memchr(const void* _Nonnull const s __pass_object_size, int c, size_t n) {
    return memchr(s, c, n);
}

inline __always_inline
const void* _Nullable memchr(const void* _Nonnull const s __pass_object_size, int c, size_t n)
        __prefer_this_overload {
    return __bionic_memchr(s, c, n);
}

inline __always_inline
void* _Nullable memchr(void* _Nonnull const s __pass_object_size, int c, size_t n) __prefer_this_overload {
    return __bionic_memchr(s, c, n);
}

inline __always_inline
char* _Nullable __bionic_strchr(const char* _Nonnull const s __pass_object_size, int c) {
    return strchr(s, c);
}

inline __always_inline
const char* _Nullable strchr(const char* _Nonnull const s __pass_object_size, int c)
        __prefer_this_overload {
    return __bionic_strchr(s, c);
}

inline __always_inline
char* _Nullable strchr(char* _Nonnull const s __pass_object_size, int c)
        __prefer_this_overload {
    return __bionic_strchr(s, c);
}

inline __always_inline
char* _Nullable __bionic_strrchr(const char* _Nonnull const s __pass_object_size, int c) {
    return strrchr(s, c);
}

inline __always_inline
const char* _Nullable strrchr(const char* _Nonnull const s __pass_object_size, int c) __prefer_this_overload {
    return __bionic_strrchr(s, c);
}

inline __always_inline
char* _Nullable strrchr(char* _Nonnull const s __pass_object_size, int c) __prefer_this_overload {
    return __bionic_strrchr(s, c);
}

/* Functions with no FORTIFY counterpart. */

inline __always_inline
char* _Nullable __bionic_strcasestr(const char* _Nonnull h, const char* _Nonnull n) {
    return strcasestr(h, n);
}

inline __always_inline
const char* _Nullable strcasestr(const char* _Nonnull h, const char* _Nonnull n) __prefer_this_overload {
    return __bionic_strcasestr(h, n);
}

inline __always_inline
char* _Nullable strcasestr(char* _Nonnull h, const char* _Nonnull n) __prefer_this_overload {
    return __bionic_strcasestr(h, n);
}

inline __always_inline
char* _Nullable __bionic_strstr(const char* _Nonnull h, const char* _Nonnull n) {
    return strstr(h, n);
}

inline __always_inline
const char* _Nullable strstr(const char* _Nonnull h, const char* _Nonnull n) __prefer_this_overload {
    return __bionic_strstr(h, n);
}

inline __always_inline
char* _Nullable strstr(char* _Nonnull h, const char* _Nonnull n) __prefer_this_overload {
    return __bionic_strstr(h, n);
}

inline __always_inline
char* _Nullable __bionic_strpbrk(const char* _Nonnull h, const char* _Nonnull n) { return strpbrk(h, n); }

inline __always_inline
char* _Nullable strpbrk(char* _Nonnull h, const char* _Nonnull n) __prefer_this_overload {
    return __bionic_strpbrk(h, n);
}

inline __always_inline
const char* _Nullable strpbrk(const char* _Nonnull h, const char* _Nonnull n) __prefer_this_overload {
    return __bionic_strpbrk(h, n);
}
}
#undef __prefer_this_overload
#endif

__END_DECLS

#endif
