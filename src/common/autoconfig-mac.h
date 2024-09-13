// This is only used on the mac when building with the .pro
// qcreator/qmake files (not for Macports or homebrew where we go the
// unix way (create autoconfig.h with configure). Copy to autoconfig.h
// before the build.

/* Path to the aspell program */
#undef ASPELL_PROG

/* No X11 session monitoring support */
#define DISABLE_X11MON 1

/* Path to the fam api include file */
/* #undef FAM_INCLUDE */

/* Path to the file program */
#define FILE_PROG "/usr/bin/file"

/* Define to 1 if you have the `kqueue' function. */
#define HAVE_KQUEUE 1

/* Define to 1 if you have the <malloc/malloc.h> header file. */
#define HAVE_MALLOC_MALLOC_H 1

/* Define to 1 if you have the `mkdtemp' function. */
#define HAVE_MKDTEMP 1

/* Define to 1 if you have the `posix_spawn' function. */
#define HAVE_POSIX_SPAWN 1

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the <spawn.h> header file. */
#define HAVE_SPAWN_H 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST 

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.40.2"

/* Use QTextBrowser to implement the preview windows */
#undef PREVIEW_FORCETEXTBROWSER

/* Compile the aspell interface */
#define RCL_USE_ASPELL 1

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif
