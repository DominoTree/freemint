/*
 * Modified for the FreeMiNT USB subsystem by David Galvez. 2010 - 2011
 *
 * XaAES - XaAES Ain't the AES (c) 1992 - 1998 C.Graham
 *                                 1999 - 2003 H.Robbers
 *                                        2004 F.Naumann & O.Skancke
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XaAES; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _global_h
#define _global_h

#include "cookie.h"
#include "mint/module.h"
#ifndef TOSONLY
#include "libkern/libkern.h"

typedef char Path[PATH_MAX];

/* XXX -> kassert */
#define FATAL KERNEL_FATAL

/* XXX -> dynamic mapping from kernel */
//#define USB_MAGIC    0x5553425FL
//#define USB_MAGIC_SH 0x55534253L

/*
 * debug section
 */

#ifdef DEV_DEBUG

# define FORCE(x)       KERNEL_FORCE x
# define ALERT(x)       KERNEL_ALERT x
# define DEBUG(x)       KERNEL_DEBUG x
# define TRACE(x)       KERNEL_TRACE x
# define ASSERT(x)      assert x

#else

# define FORCE(x)       KERNEL_FORCE x
# define ALERT(x)       KERNEL_ALERT x
# define DEBUG(x)
# define TRACE(x)
# define ASSERT(x)      assert x

#endif

#else /* TOSONLY */
#include <mint/osbind.h> /* Setexc */
#include <stdarg.h>

/* BIOS */

/* DavidGZ: changedrv doesn't seem equivalent to Mediach BIOS function.
 * I don't know why this was done, just in case I'm missing something
 * I've commented the define instead of removing it.
 */
#undef changedrv
#define changedrv(x) /* (void)Mediach */
#undef b_setexc
#define b_setexc Setexc

/* XBIOS */
#undef s_version
#define s_version Sversion
#undef b_kbdvbase
#define b_kbdvbase Kbdvbase
#undef b_uiorec
#define b_uiorec Iorec
#undef b_supexec
#define b_supexec(x0,x1,x2,x3,x4,x5) Supexec(x0)

/* GEMDOS */
#undef c_conws
#define c_conws (void)Cconws
#undef c_conout
#define c_conout (void)Cconout
#undef d_setdrv
#define d_setdrv (void)Dsetdrv
#undef kmalloc
#define kmalloc Malloc
#undef kfree
#define kfree Mfree
#undef f_open
#define f_open Fopen
#undef f_read
#define f_read Fread
#undef f_close
#define f_close Fclose


/* library declarations from libkern */

# if __KERNEL__ == 3	/* These declarations are only needed for TOS drivers,
						 * usbtool.acc is linked against LIBCMINI
						 */
void *	_cdecl memcpy		(void *dst, const void *src, unsigned long nbytes);
void *	_cdecl memset		(void *dst, int ucharfill, unsigned long size);

long	_cdecl _mint_strncmp	(const char *str1, const char *str2, long len);
char *	_cdecl _mint_strncpy	(char *dst, const char *src, long len);
char *	_cdecl _mint_strcat	(char *dst, const char *src);
long	_cdecl _mint_strlen	(const char *s);

long	_cdecl kvsprintf(char *buf, long buflen, const char *fmt, va_list args) __attribute__((format(printf, 3, 0)));
long	_cdecl ksprintf		(char *buf, long buflen, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

# define strlen			_mint_strlen
# define strncmp		_mint_strncmp
# define strncpy		_mint_strncpy
# define strcat			_mint_strcat

#define sprintf			ksprintf

#endif /* __KERNEL__ */

#ifdef DEV_DEBUG

/* Debug console output for TOS */
static char tos_debugbuffer[512];

static void tos_printmsg(const char *fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	kvsprintf (tos_debugbuffer, sizeof(tos_debugbuffer)-1, fmt, args);
	va_end (args);
	(void)Cconws(tos_debugbuffer);
	(void)Cconws("\r\n");
}

# define ALERT(x)       tos_printmsg x
# define DEBUG(x)       tos_printmsg x
# define TRACE(x)
# define ASSERT(x)

#else /* !DEV_DEBUG */

# define ALERT(x)
# define DEBUG(x)
# define TRACE(x)
# define ASSERT(x)
#endif /* !DEV_DEBUG */


/* cookie jar definition
 */

#define _USB 0x5f555342L

static inline int getcookie(long target,long *p_value)
{
	long oldssp;
	struct cookie *cookie_ptr;

	if (Super((void *)1L) == 0L)
		oldssp = Super(0L);
	else
		oldssp = 0;

	cookie_ptr = *CJAR;

	if (oldssp)
		SuperToUser((void *)oldssp);

	if (cookie_ptr) {
		do {
			if (cookie_ptr->tag == target) {
				if (p_value)
					*p_value = cookie_ptr->value;
				return 1;
			}
		} while ((cookie_ptr++)->tag != 0L);
	}

	return 0;
}

static inline int setcookie (long tag, long value)
{
	long oldssp;
	struct cookie *cjar;
	long n = 0;

	if (Super((void *)1L) == 0L)
		oldssp = Super(0L);
	else
		oldssp = 0;

	cjar = * CJAR;

	if (oldssp)
		SuperToUser((void *)oldssp);

	while (cjar->tag)
	{
		n++;
		if (cjar->tag == tag)
		{
			cjar->value = value;
			return 1;
		}
		cjar++;
	}

	n++;
	if (n < cjar->value)
	{
		n = cjar->value;
		cjar->tag = tag;
		cjar->value = value;

		cjar++;
		cjar->tag = 0L;
		cjar->value = n;
		return 1;
	}

	return 0;
}

/* Precise delays functions for TOS USB drivers */
#include "tosdelay.c"

#endif /* TOSONLY */

static inline void hex_nybble(int n)
{
	char c;

	c = (n > 9) ? 'A'+n-10 : '0'+n;
	c_conout(c);
}

static inline void hex_byte(uchar n)
{
	hex_nybble(n>>4);
	hex_nybble(n&0x0f);
}

static inline void hex_word(ushort n)
{
	hex_byte(n>>8);
	hex_byte(n&0xff);
}

static inline void hex_long(ulong n)
{
	hex_word(n>>16);
	hex_word(n&0xffff);
};

#endif /* _global_h */
