#include <stdio.h>	/* printf() puts() */
#include <stdlib.h>	/* exit() */

#include "types.h"
#include "loader.h"

__attribute__((cold noreturn always_inline)) static inline
void usage (char *myself) {
	printf("Usage: ... %s SAMPLER ARGS\n", myself);
	puts("	SAMPLER	shared object file containing complex sampler function");
	puts("	ARGS	any extra arguments required by the complex sampler");
	exit(0);
}

static long double (*complex_sample) (
	long double complex *z, long double complex *c
);

__attribute__((cold))
void init (char **argv) {
	/* count args */
	int argc = 0;
	while (argv[++argc]);
	if (2 > argc) usage(argv[0]);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"
	complex_sample = (long double (*) (
		long double complex *z, long double complex *c
	))get_sampler(&argv[1]);
#pragma GCC diagnostic pop
}

__attribute__((pure hot))
long double sample (long double complex *point) {
	static long double complex zero = 0 + 0 * I;
	return complex_sample(&zero, point);
}