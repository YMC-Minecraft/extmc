#include "threads_util.h"

#if defined(__linux__)
#include <sys/prctl.h>
#endif

void thread_set_name(const char *name)
{
#if defined(__linux__)
	/* Use prctl instead to prevent using _GNU_SOURCE flag and implicit declaration */
	prctl(PR_SET_NAME, name);
#elif defined(__APPLE__) && defined(__MACH__)
	pthread_setname_np(name);
#endif
}
