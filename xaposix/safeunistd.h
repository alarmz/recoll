/* safeunistd.h: <unistd.h>, but with compat. and large file support for MSVC.
 *
 * Copyright (C) 2007 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef XAPIAN_INCLUDED_SAFEUNISTD_H
#define XAPIAN_INCLUDED_SAFEUNISTD_H

#ifndef _MSC_VER
# include <unistd.h>
#define sys_read ::read
#define sys_write ::write

#else

// sys/types.h has a typedef for off_t so make sure we've seen that before
// we hide it behind a #define.
# include <sys/types.h>

// MSVC doesn't even HAVE unistd.h - io.h seems the nearest equivalent.
// We also need to do some renaming of functions to get versions which
// work on large files.
# include <io.h>

# ifdef lseek
#  undef lseek
# endif

# ifdef off_t
#  undef off_t
# endif

# define lseek(FD, OFF, WHENCE) _lseeki64(FD, OFF, WHENCE)
# define off_t __int64

#endif

#ifdef __WIN32__

#ifndef _SSIZE_T_DEFINED
#ifdef  _WIN64
typedef __int64    ssize_t;
#else
typedef int   ssize_t;
#endif
#define _SSIZE_T_DEFINED
#endif

inline ssize_t sys_read(int fd, void* buf, size_t cnt)
{
    return static_cast<ssize_t>(::read(fd, buf, static_cast<int>(cnt)));
}
inline ssize_t sys_write(int fd, const void* buf, size_t cnt)
{
    return static_cast<ssize_t>(::write(fd, buf, static_cast<int>(cnt)));
}

#endif /* __WIN32__ */

#endif /* XAPIAN_INCLUDED_SAFEUNISTD_H */
