/* Manually edited version of autoconfig.h for windows. Many things are
   overriden in the c++ code by ifdefs _WIN32 anyway  */

/* Aspell program parameter to findFilter(). */
#define ASPELL_PROG "aspell-installed/mingw32/bin/aspell"

/* Use libmagic */
#define ENABLE_LIBMAGIC 1

/* No X11 session monitoring support */
#define DISABLE_X11MON

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Use multiple threads for indexing */
#undef IDX_THREADS

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.43.4pre1"

/* Use QTextBrowser to implement the preview windows */
#undef PREVIEW_FORCETEXTBROWSER

/* Real time monitoring option */
#define RCL_MONITOR 1

/* Compile the aspell interface */
#define RCL_USE_ASPELL 1
