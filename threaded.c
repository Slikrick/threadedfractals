/* threaded.c, from threadedfractals
 * by "Pegasus Epsilon" <pegasus@pimpninjas.org>
 * Distribute Unmodified - http://pegasus.pimpninjas.org/license
 */

#include <stdio.h>  	/* printf(), puts(), fopen(), fwrite() */
#include <stdlib.h> 	/* strtold() */
#include <stdarg.h> 	/* va_list, va_start(), va_end() */
#include <stdbool.h>	/* bool, true, false */
#include <pthread.h>	/* pthread_* */

#include "circularlist.h"
#include "modules/sampler.h"
#include "loader.h"
#include "mapper.h"
#include "types.h"
#include "utils.h"

#define line_t FLOAT[max.real]

struct pixel max;
list output_buffer;
unsigned long long thread_count, *queue, next_line = 0;

__attribute__((hot, always_inline)) static inline
void display (void) {
	/* queue */
	printf("Q: ");
	for (unsigned long long i = 0; i < thread_count; i++)
		printf("%llu, ", queue[i]);
	/* progress */
	printf("P: %llu/%llu (%0.2" FMT "%%), ", next_line, max.imag,
		100 * (FLOAT)next_line / max.imag);
	/* buffer */
	unsigned long long used = list_used(output_buffer);
	unsigned long long length = list_length(output_buffer);
	printf("B: %llu/%llu (%0.2" FMT "%%)\x1b[K\r", used, length, 100 * (FLOAT)used / length);
	fflush(stdout);
}

pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t data_written = PTHREAD_COND_INITIALIZER;
FILE *output_file;
__attribute__((hot, always_inline)) static inline
unsigned long long output (list_buffer *line) {
	pthread_mutex_lock(&write_lock);

	while ((*line = list_read(output_buffer))) {
		fwrite(**line, sizeof(FLOAT), max.real, output_file);
		pthread_cond_broadcast(&data_written);
	}

	if (next_line == max.imag) {
		/* no work left */
		pthread_mutex_unlock(&write_lock);
		return -1;
	}

	*line = list_get_write_ptr(output_buffer);
	if (NULL == **line) **line = new(line_t);

	display();

	/* remember to increment inside the loop, which requires returning a copy */
	unsigned long long copy = next_line++;

	pthread_mutex_unlock(&write_lock);

	return copy;
}

complex FLOAT pixelsize;
complex FLOAT ratio;
static sampler(sample);
__attribute__((hot, always_inline)) static inline
void iterate_line (list_buffer *line, unsigned long long imag) {
	complex FLOAT point;
	struct pixel this = { .imag = imag };

	for (this.real = 0; this.real < max.real; this.real++) {
		point = pixel2vector(&this, &pixelsize, &ratio);
		((FLOAT *)**line)[this.real] = sample(&point);
	}

	list_mark_ready(*line);
}

list_buffer *thread_buffers;
static void *thread (void *ptr) {
	unsigned long long i = (unsigned long long)ptr;
	list_buffer *line = &thread_buffers[i];
	while ((unsigned long long)-1 != queue[i]) {
		iterate_line(line, queue[i]);
		queue[i] = output(line);
	}
	queue[i] = max.imag;
	return NULL;
}

__attribute__((cold, noreturn, always_inline)) static inline
void usage (char *myself) {
	puts("Threaded fractal sampler\n");
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
	max.real = atoi(argv[2]);
	max.imag = atoi(argv[3]);
	/* reverse vertical axis so positive = up */
	ratio = 1 - (FLOAT)max.imag / max.real * I;
	output_file = fopen(argv[4], "w");

	/* cache some math */
	pixelsize = calculate_pixelsize(&max, &ratio);

	/* allocate process tracking space */
	queue = new(unsigned long long [thread_count]);
	pthread_t *threads = new(pthread_t[thread_count]);

	printf("spinning up %llu threads\n", thread_count);

	/* allocate output buffer */
	output_buffer = new_list(thread_count);
	thread_buffers = new(list_buffer[thread_count]);

	next_line = thread_count;
	for (unsigned long long i = 0; i < thread_count; i++) {
		thread_buffers[i] = list_get_write_ptr(output_buffer);
		*(thread_buffers[i]) = new(line_t);
		queue[i] = i;
		pthread_create(&threads[i], NULL, thread, (void *)i);
	}
	display();

	for (unsigned long long i = 0; i < thread_count; i++)
		while (pthread_join(threads[i], NULL));

	display();
	puts("\nDone!");
	fflush(stdout);

	free(threads);
	free(queue);
	free(thread_buffers);
	delete_list(output_buffer);
	fclose(output_file);

	return 0;
}
