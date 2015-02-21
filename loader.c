#define _GNU_SOURCE 	/* asprintf() */

#include <stdlib.h> 	/* atexit() */
#include <dlfcn.h>  	/* dlopen(), dlsym(), dlclose() */

#include "types.h"
#include "utils.h"

void *sampler_handle = NULL;
__attribute__((cold)) static
void dispose_sampler (void) { dlclose(sampler_handle); }

__attribute__((cold))
long double (*get_sampler (char *lib_name))(struct coordinates_4d *) {
	long double (*fn)(struct coordinates_4d *);

	if (sampler_handle)
		die("Calling get_sampler twice will result in leaks. Exiting.");

	if (!(sampler_handle = dlopen(lib_name, RTLD_LAZY))) {
		char *tmp;
		asprintf(&tmp, "./%s", lib_name);
		sampler_handle = dlopen(tmp, RTLD_LAZY);
		free(tmp);
		if (!sampler_handle) die(dlerror());
	}

	/* clean up when we exit */
	atexit(dispose_sampler);

/* We have officially left the realm of portability.
 * To port this to Windows will now require DLL shenanigans and some #ifdefs.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"
	if (!(fn = (long double (*)(struct coordinates_4d *))dlsym(sampler_handle, "sample"))) {
#pragma GCC diagnostic pop
		char *tmp = dlerror();
		die(tmp ? tmp : "NULL sampler not allowed");
	}

	return fn;
}
