/* utils.c, from threadedfractals
 * by "Pegasus Epsilon" <pegasus@pimpninjas.org>
 * Distribute Unmodified - http://pegasus.pimpninjas.org/license
 */

#include <stdlib.h>   	/* exit() */
#include <stdarg.h>   	/* va_list, va_start(), vprintf(), va_end() */
#include <stdio.h>    	/* perror() puts() */
#include <stdbool.h>  	/* bool */

/* Print errno error message and exit with errorlevel */
__attribute__((cold, noreturn))
void fail (const char *restrict const msg) {
	perror(msg);
	exit(1);
}

/* Print custom message and exit with errorlevel */
__attribute__((cold, noreturn))
void die (const char *restrict const fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	puts(".");
	exit(1);
}

/* debug print system */
int debug_off (const char *restrict const fmt, ...) { return (long)fmt; }
int debug_on  (const char *restrict const fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int ret = vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
	return ret;
}
int (*debug)(const char *, ...) = &debug_off;
void enable_debug (void) { debug = &debug_on; }
bool debug_enabled (void) { return debug == &debug_on; }
