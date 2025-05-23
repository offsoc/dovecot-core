/* Copyright (c) 2001-2018 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "dovecot-version.h"
#include "array.h"
#include "event-filter.h"
#include "env-util.h"
#include "hostpid.h"
#include "ipwd.h"
#include "process-title.h"
#include "restrict-access.h"
#include "randgen.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef HAVE_FACCESSAT2
#  include <asm/unistd.h>
#endif

/* Mainly for including the full version information in core dumps.
   NOTE: Don't set this const - otherwise it won't end up in core dumps. */
char dovecot_build_info[] = DOVECOT_BUILD_INFO;

static bool lib_initialized = FALSE;
int dev_null_fd = -1;

struct atexit_callback {
	int priority;
	lib_atexit_callback_t *callback;
};

static ARRAY(struct atexit_callback) atexit_callbacks = ARRAY_INIT;
static bool lib_clean_exit;

/* The original faccessat() syscall didn't handle the flags parameter.  glibc
   v2.33's faccessat() started using the new Linux faccessat2() syscall for that
   reason.  However, we can still use the faccessat2() syscall directly in some
   Linux distros to avoid this problem, so just do it here when possible. */
int i_faccessat2(int dirfd, const char *pathname, int mode, int flags)
{
#ifdef HAVE_FACCESSAT2
	static bool faccessat2_unavailable = FALSE;
	if (!faccessat2_unavailable) {
		/* On bullseye the syscall is available,
		   but the glibc wrapping function is not. */
		long ret = syscall(__NR_faccessat2, dirfd, pathname, mode, flags);
		faccessat2_unavailable = ret == -1 && errno == ENOSYS;
		if (!faccessat2_unavailable)
			return (int)ret;
	}
#endif
	return faccessat(dirfd, pathname, mode, flags);
}

#undef i_unlink
int i_unlink(const char *path, const char *source_fname,
	     unsigned int source_linenum)
{
	if (unlink(path) < 0) {
		i_error("unlink(%s) failed: %m (in %s:%u)",
			path, source_fname, source_linenum);
		return -1;
	}
	return 0;
}

#undef i_unlink_if_exists
int i_unlink_if_exists(const char *path, const char *source_fname,
		       unsigned int source_linenum)
{
	if (unlink(path) == 0)
		return 1;
	else if (errno == ENOENT)
		return 0;
	else {
		i_error("unlink(%s) failed: %m (in %s:%u)",
			path, source_fname, source_linenum);
		return -1;
	}
}

void i_getopt_reset(void)
{
#ifdef __GLIBC__
	/* a) for subcommands allow -options anywhere in command line
	   b) this is actually required for the reset to work (glibc bug?) */
	optind = 0;
#else
	optind = 1;
#endif
}

void lib_atexit(lib_atexit_callback_t *callback)
{
	lib_atexit_priority(callback, 0);
}

void lib_atexit_priority(lib_atexit_callback_t *callback, int priority)
{
	struct atexit_callback *cb;
	const struct atexit_callback *callbacks;
	unsigned int i, count;

	if (!array_is_created(&atexit_callbacks))
		i_array_init(&atexit_callbacks, 8);
	else {
		/* skip if it's already added */
		callbacks = array_get(&atexit_callbacks, &count);
		for (i = count; i > 0; i--) {
			if (callbacks[i-1].callback == callback) {
				i_assert(callbacks[i-1].priority == priority);
				return;
			}
		}
	}
	cb = array_append_space(&atexit_callbacks);
	cb->priority = priority;
	cb->callback = callback;
}

static int atexit_callback_priority_cmp(const struct atexit_callback *cb1,
					const struct atexit_callback *cb2)
{
	return cb1->priority - cb2->priority;
}

void lib_atexit_run(void)
{
	const struct atexit_callback *cb;

	if (array_is_created(&atexit_callbacks)) {
		array_sort(&atexit_callbacks, atexit_callback_priority_cmp);
		array_foreach(&atexit_callbacks, cb)
			(*cb->callback)();
		array_free(&atexit_callbacks);
	}
}

static void lib_open_non_stdio_dev_null(void)
{
	dev_null_fd = open("/dev/null", O_WRONLY);
	if (dev_null_fd == -1)
		i_fatal("open(/dev/null) failed: %m");
	/* Make sure stdin, stdout and stderr fds exist. We especially rely on
	   stderr being available and a lot of code doesn't like fd being 0.
	   We'll open /dev/null as write-only also for stdin, since if any
	   reads are attempted from it we'll want them to fail. */
	while (dev_null_fd < STDERR_FILENO) {
		dev_null_fd = dup(dev_null_fd);
		if (dev_null_fd == -1)
			i_fatal("dup(/dev/null) failed: %m");
	}
	/* close the actual /dev/null fd on exec*(), but keep it in stdio fds */
	fd_close_on_exec(dev_null_fd, TRUE);
}

void lib_set_clean_exit(bool set)
{
	lib_clean_exit = set;
}

void lib_exit(int status)
{
	lib_set_clean_exit(TRUE);
	exit(status);
}

static void lib_atexit_handler(void)
{
	/* We're already in exit code path. Avoid using any functions that
	   might cause strange breakage. Especially anything that could call
	   exit() again could cause infinite looping in some OSes. */
	if (!lib_clean_exit) {
		const char *error = "Unexpected exit - converting to abort\n";
		if (write(STDERR_FILENO, error, strlen(error)) < 0) {
			/* ignore */
		}
		abort();
	}
}

void lib_init(void)
{
	i_assert(!lib_initialized);
	random_init();
	data_stack_init();
	hostpid_init();
	lib_open_non_stdio_dev_null();
	lib_event_init();
	event_filter_init();

	/* Default to clean exit. Otherwise there would be too many accidents
	   with e.g. command line parsing errors that try to return instead
	   of using lib_exit(). master_service_init_finish() will change this
	   again to be FALSE. */
	lib_set_clean_exit(TRUE);
	atexit(lib_atexit_handler);

	lib_initialized = TRUE;
}

bool lib_is_initialized(void)
{
	return lib_initialized;
}

void lib_deinit(void)
{
	i_assert(lib_initialized);
	lib_initialized = FALSE;
	lib_atexit_run();
	ipwd_deinit();
	hostpid_deinit();
	event_filter_deinit();
	data_stack_deinit_event();
	lib_event_deinit();
	restrict_access_deinit();
	i_close_fd(&dev_null_fd);
	data_stack_deinit();
	failures_deinit();
	process_title_deinit();
	random_deinit();

	lib_clean_exit = TRUE;
}
