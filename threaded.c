#define _GNU_SOURCE 	/* asprintf() */

#include <stdio.h>  	/* printf(), puts(), fopen(), fwrite() */
#include <stdlib.h> 	/* strtold() */
#include <stdarg.h> 	/* va_list, va_start(), va_end() */
#include <pthread.h>	/* pthread_* */
#include <stdbool.h>	/* bool, true, false */

#define RADIUS 2.2

#include "modules/sampler.h"
#include "loader.h"
#include "mapper.h"
#include "types.h"

struct line { long double *data; bool ready; bool assigned; };

struct line *buffer_start, *buffer_end, *buffer_read, *buffer_write;
bool buffer_full = false;
FILE *output_file;
unsigned long long buffer_size, thread_count, *queue, next_line = 0, buffer_waiting = 0;
struct pixel max;

__attribute__((hot always_inline)) static inline
void display (void) {
	// This takes nonzero time to draw, but it looks neat.
	struct line *tmp = buffer_start;
	unsigned long long lim = (buffer_size / thread_count) >> 1;
	putchar('[');
	for (unsigned long long i = 0; i < buffer_size; i++) {
		if (i && 0 == (i % lim)) printf("]\n[");
		putchar(tmp[i].ready ? '=' : tmp[i].assigned ? '|' : ' ');
	}
	puts("]");
	for (unsigned long long i = 0; i < thread_count; i++)
		printf("%llu, ", queue[i]);
	printf(
		"%llu/%llu (%02.02f%%)\x1b[K\x1b[%lluA\r",
		next_line, max.imag, next_line / (float)max.imag * 100, thread_count << 1
	);
	/*/
	// This is somewhat faster, but not as sweet looking.
	printf("progress: %llu/%llu (%02.02f%% done), buffer: %llu/%llu (%02.02f%% full)\x1b[K\r",
		next_line, max.imag, next_line / (float)max.imag * 100,
		buffer_waiting, buffer_size, buffer_waiting / (float)buffer_size * 100
	);
	// */

	fflush(stdout);
}

pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t data_written = PTHREAD_COND_INITIALIZER;
FILE *output_file;
__attribute__((hot always_inline)) static inline
unsigned long long output (struct line **line) {
	pthread_mutex_lock(&write_lock);

	(*line)->assigned = false;

	buffer_waiting++;

	while (buffer_read->ready) {
		buffer_waiting--;
		buffer_full = false;
		buffer_read->ready = false;
		fwrite(buffer_read->data, sizeof(long double), max.real, output_file);
		if (++buffer_read == buffer_end) buffer_read = buffer_start;
		pthread_cond_broadcast(&data_written);
	}

	while (buffer_full) pthread_cond_wait(&data_written, &write_lock);

	if (next_line == max.imag) {
		/* no work left */
		pthread_mutex_unlock(&write_lock);
		return -1;
	}

	(*line) = buffer_write;
	(*line)->assigned = true;

	display();

	if (++buffer_write == buffer_end) buffer_write = buffer_start;
	if (buffer_write == buffer_read) buffer_full = true;

	/* remember to increment inside the loop, which requires returning a copy */
	unsigned long long copy = next_line++;

	pthread_mutex_unlock(&write_lock);

	return copy;
}

long double complex pixelsize;
long double complex radius;
static sampler(sample);
__attribute__((hot always_inline)) static inline
struct line **iterate_line (
	struct line **line, unsigned long long imag
) {
	long double complex point;
	struct pixel this = { .imag = imag };

	for (this.real = 0; this.real < max.real; this.real++) {
		point = pixel2vector(&this, &pixelsize, &radius);
		(*line)->data[this.real] = sample(&point);
	}
	(*line)->ready = true;
	return line;
}

static void *thread (void *ptr) {
	unsigned long long thread = (unsigned long long)ptr;
	struct line *line = buffer_start + thread;
	queue[thread] = thread;
	while ((unsigned long long)-1 != (
		queue[thread] = output(iterate_line(&line, queue[thread]))
	));
	queue[thread] = max.imag;
	return NULL;
}

__attribute__((cold noreturn always_inline)) static inline
void usage (char *myself) {
	puts("Threaded Mandelbrot sampler\n");
	printf("Usage: %s THREADS WIDTH HEIGHT OUTFILE SAMPLER ARGS\n\n", myself);
	puts("	THREADS	how many threads to spawn");
	puts("	WIDTH	number of horizontal samples");
	puts("	HEIGHT	number of vertical samples");
	puts("	OUTFILE	name of file to write output to");
	puts("	SAMPLER	shared object file containing sampler function");
	puts("	ARGS	any extra arguments required by the sampler");
	puts("\nReport bugs to pegasus@pimpninjas.org");
	exit(1);
}

int main (int argc, char **argv) {

	if (6 > argc) usage(argv[0]);

	sample = get_sampler(&argv[5]);

	thread_count = atoi(argv[1]);
	buffer_size = thread_count << 7;
	max.real = atoi(argv[2]);
	max.imag = atoi(argv[3]);
	radius = RADIUS + RADIUS * max.imag / max.real * I;
	output_file = fopen(argv[4], "w");

	/* cache some math */
	pixelsize = calculate_pixelsize(&max, &radius);

	/* allocate output buffer */
	buffer_read = buffer_start = calloc(buffer_size, sizeof(struct line));
	buffer_write = buffer_read + thread_count;
	buffer_end = buffer_start + buffer_size;
	for (unsigned long long i = 0; i < buffer_size; i++)
		buffer_start[i].data = calloc(max.real, sizeof(long double));

	/* allocate process tracking space */
	queue = calloc(thread_count, sizeof(unsigned long long));
	pthread_t *threads = calloc(thread_count, sizeof(pthread_t));

	printf("spinning up %llu threads\n", thread_count);
	for (unsigned long long i = 0; i < thread_count; i++) {
		buffer_start[i].assigned = true;
		queue[i] = i;
	}
	display();

	next_line = thread_count;
	for (unsigned long long i = 0; i < thread_count; i++)
		pthread_create(&threads[i], NULL, &thread, (void *)i);

	for (unsigned long long i = 0; i < thread_count; i++)
		while (!pthread_join(threads[i], NULL));

	display();
	thread_count <<= 1;
	for (unsigned long long i = 0; i <= thread_count; i++) putchar('\n');
	puts("Done!");
	fflush(stdout);

	free(threads);
	free(queue);
	free(buffer_start);
	fclose(output_file);

	return 0;
}
