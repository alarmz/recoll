/* Manually edited version of autoconfig.h for windows. Many things are
   overriden in the c++ code by ifdefs _WIN32 anyway  */

/* Aspell program parameter to findFilter(). */
#define ASPELL_PROG "aspell-installed/mingw32/bin/aspell"

/* Use libmagic */
#define ENABLE_LIBMAGIC 1

/* No X11 session monitoring support */
#define DISABLE_X11MON

/* Path to the file program */
#define FILE_PROG "/usr/bin/file"

/* Define to 1 if you have the `kqueue' function. */
#undef HAVE_KQUEUE

/* Define to 1 if you have the <malloc.h> header file. */
#undef HAVE_MALLOC_H

/* Define to 1 if you have the <malloc/malloc.h> header file. */
#undef HAVE_MALLOC_MALLOC_H

/* Define to 1 if you have the `malloc_trim' function. */
#undef HAVE_MALLOC_TRIM

/* Define to 1 if you have the `mkdtemp' function. */
#undef HAVE_MKDTEMP

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the <spawn.h> header file. */
#define HAVE_SPAWN_H 1

/* Define to 1 if you have the `vsnprintf' function. */
#undef HAVE_VSNPRINTF

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Use multiple threads for indexing */
#define IDX_THREADS 1

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.42.0pre2"

/* Use QTextBrowser to implement the preview windows */
#undef PREVIEW_FORCETEXTBROWSER

/* Real time monitoring option */
#define RCL_MONITOR 1

/* Split camelCase words */
/* #undef RCL_SPLIT_CAMELCASE */

/* Compile the aspell interface */
#define RCL_USE_ASPELL 1

/* Compile the fam interface */
/* #undef RCL_USE_FAM */

/* Compile the inotify interface */
/* #undef RCL_USE_INOTIFY */

/* Use posix_spawn() */
/* #undef USE_POSIX_SPAWN */

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */
