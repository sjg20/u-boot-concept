/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_STRING_H_
#define _LINUX_STRING_H_

#include <linux/compiler.h>	/* for inline */
#include <linux/types.h>	/* for size_t */
#include <linux/stddef.h>	/* for NULL */
#include <linux/errno.h>	/* for E2BIG */

/*
 * Include machine specific inline routines
 */
#include <asm/string.h>

#ifndef __HAVE_ARCH_STRCPY
extern char * strcpy(char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRNCPY
extern char * strncpy(char *,const char *, __kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif
#ifndef __HAVE_ARCH_STRCAT
extern char * strcat(char *, const char *);
#endif
#ifndef __HAVE_ARCH_STRNCAT
extern char * strncat(char *, const char *, __kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRLCAT
extern size_t strlcat(char *, const char *, __kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRCMP
extern int strcmp(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRNCMP
extern int strncmp(const char *,const char *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
#endif
#ifndef __HAVE_ARCH_STRNCASECMP
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#ifndef __HAVE_ARCH_STRCHR
extern char * strchr(const char *,int);
#endif
extern char * strchrnul(const char *,int);
#ifndef __HAVE_ARCH_STRRCHR
extern char * strrchr(const char *,int);
#endif

#include <linux/linux_string.h>

#ifndef __HAVE_ARCH_STRSTR
extern char * strstr(const char *, const char *);
#endif
#ifndef __HAVE_ARCH_STRLEN
extern __kernel_size_t strlen(const char *);
#endif
#ifndef __HAVE_ARCH_STRNLEN
extern __kernel_size_t strnlen(const char *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_STRPBRK
extern char * strpbrk(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRSEP
extern char * strsep(char **,const char *);
#endif
#ifndef __HAVE_ARCH_STRSPN
extern __kernel_size_t strspn(const char *,const char *);
#endif
#ifndef __HAVE_ARCH_STRCSPN
extern __kernel_size_t strcspn(const char *,const char *);
#endif

#ifndef __HAVE_ARCH_MEMSET
extern void * memset(void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCPY
extern void * memcpy(void *,const void *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMMOVE
extern void * memmove(void *,const void *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMSCAN
extern void * memscan(void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCMP
extern int memcmp(const void *,const void *,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCHR
extern void * memchr(const void *,int,__kernel_size_t);
#endif
#ifndef __HAVE_ARCH_MEMCHR_INV
void *memchr_inv(const void *s, int c, size_t n);
#endif

/* U-Boot specific APIs */
extern char *strtok(char *, const char *);
#ifdef CONFIG_SANDBOX
# define strdup		sandbox_strdup
# define strndup		sandbox_strndup
#endif

#ifndef __HAVE_ARCH_STRDUP
extern char *strdup(const char *);
extern char *strndup(const char *, size_t);
#endif
#ifndef __HAVE_ARCH_STRSWAB
extern char *strswab(const char *);
#endif

/**
 * memdup() - allocate a buffer and copy in the contents
 *
 * Note that this returns a valid pointer even if @len is 0
 *
 * @src: data to copy in
 * @len: number of bytes to copy
 * Return: allocated buffer with the copied contents, or NULL if not enough
 *	memory is available
 *
 */
char *memdup(const void *src, size_t len);

unsigned long ustrtoul(const char *cp, char **endp, unsigned int base);
unsigned long long ustrtoull(const char *cp, char **endp, unsigned int base);

#endif /* _LINUX_STRING_H_ */
