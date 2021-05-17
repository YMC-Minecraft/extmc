#include "thpool.h"
#include "plugins.h"
#include "plugin_registry.h"
#include "mcin.h"
#include "common.h"
#include "rcon_host.h"
#include "threads_util.h"

#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <semaphore.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <inttypes.h>
#include <limits.h>

#ifndef CONTROL_SOCKET_PATH
#define CONTROL_SOCKET_PATH "/run/extmc.ctl"
#endif

static pthread_mutex_t process_mutex = PTHREAD_MUTEX_INITIALIZER;
static threadpool thpool = NULL;
static bool received_sigterm = false;
static sem_t exit_sem;
static int ctl_fd = -1;

static void exclusive_section_enter()
{
	pthread_mutex_lock(&process_mutex);
}

static void exclusive_section_leave()
{
	pthread_mutex_unlock(&process_mutex);
}

static int autoload(const char *config_path)
{
	FILE *file = fopen(config_path, "r");
	if(file == NULL)
	{
		char buf[128];
		int r = errno;
		strerror_r(r, buf, 128);
		fprintf(stderr, _("Cannot open %s: %s.\n"), config_path, buf);
		return r;
	}
	int r = 0;
	while(true)
	{
		char *current_path = calloc(4098, sizeof(char));
		if(current_path == NULL)
		{
			r = errno;
			fprintf(stderr, _("Cannot allocate memory: %d.\n"), r);
			fclose(file);
			return r;
		}
		for(unsigned int i = 2; i <= UINT_MAX; i ++)
		{
			if(fgets(&current_path[(i - 2) * 4097], 4098, file) == NULL)
			{
				free(current_path);
				current_path = NULL;
				break;
			}
			if(current_path[strlen(current_path) - 1] != '\n')
			{
				char *current_path_ext = realloc(current_path, 4098 * sizeof(char) * i);
				if(current_path_ext == NULL)
				{
					r = errno;
					fprintf(stderr, _("Cannot allocate memory: %d.\n"), r);
					free(current_path);
					fclose(file);
					return r;
				}
				current_path = current_path_ext;
			}
			else
			{
				break;
			}
		}
		if(current_path == NULL) break;
		// Remove \n
		current_path[strlen(current_path) - 1] = '\0';
		if(strlen(current_path) > 0)
		{
			int id = 0;
			if(!plugin_registry_load(2, current_path, &id))
				printf(_("Autoload: loaded %s with ID %d.\n"), current_path, id);
		}
		free(current_path);
	}
	fclose(file);
	return 0;
}

static void *main_sighandler(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	thread_set_name("sighandler");
	int r = 0;
	sigset_t *set = arg;
	int sig;

	while(true)
	{
		r = sigwait(set, &sig);
		if(r)
		{
			fprintf(stderr, _("sigwait(): %d\n"), r);
			goto cleanup;
		}
		switch(sig)
		{
			case SIGINT:
			case SIGTERM:
				printf(_("Received SIGINT or SIGTERM. Exiting.\n"));
				received_sigterm = true;
				goto cleanup;
		}
	}
	goto cleanup;
cleanup:
	sem_post(&exit_sem);
	pthread_exit(NULL);
	return NULL;
}

static void *main_loop(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	thread_set_name("main-loop");
	char buffer[501];
	while(true)
	{
		if(fgets(buffer, 500, stdin) == NULL)
		{
			printf(_("Received EOF. Exiting.\n"));
			goto cleanup;
		}
		exclusive_section_enter();
		if(received_sigterm)
		{
			exclusive_section_leave();
			goto cleanup;
		}
		mcin_match(buffer, thpool);
		exclusive_section_leave();
	}
	goto cleanup;
cleanup:
	sem_post(&exit_sem);
	pthread_exit(NULL);
	return NULL;
}

static int main_handle_cmd(const int out, int argc, char **argv)
{
	if(argc <= 0)
	{
		dprintf(out, _("Invalid arguments\n"));
		return 64;
	}
	if(!strcmp(argv[0], "list"))
	{
		if(argc != 1)
		{
			dprintf(out, _("list expects no arguments\n"));
			return 64;
		}
		for(int i = 0; i < plugin_size(); i ++)
		{
			const struct plugin *plug = plugin_get_by_index(i);
			dprintf(out, _("%d\t%s\n"), plug->id, plug->name);
		}
		return 0;
	}
	if(!strcmp(argv[0], "load"))
	{
		if(argc != 2)
		{
			dprintf(out, _("load expects one argument: load <path/to/lib.so>\n"));
			return 64;
		}
		dprintf(out, _("Waiting until the processing is done.\n"));
		exclusive_section_enter();
		thpool_wait(thpool);
		int id = -1;
		int r = plugin_registry_load(out, argv[1], &id);
		if(!r)
		{
			dprintf(out, _("ID: %d\n"), id);
		}
		exclusive_section_leave();
		return r;
	}
	if(!strcmp(argv[0], "unload"))
	{
		if(argc != 2)
		{
			dprintf(out, _("unload expects one argument: unload <ID>\n"));
			return 64;
		}
	        char *endptr;
	        intmax_t num = strtoimax(argv[1], &endptr, 10);
	        if(strcmp(endptr, "") || (num == INTMAX_MAX && errno == ERANGE) || num > INT_MAX || num < INT_MIN)
		{
			dprintf(out, _("Invalid ID: %s\n"), argv[1]);
			return 64;
		}
		int id = (int)num;
		dprintf(out, _("Waiting until the processing is done.\n"));
		int r = 0;
		exclusive_section_enter();
		thpool_wait(thpool);
		struct plugin *plug = plugin_get(id);
		if(plug == NULL)
		{
			r = 1;
			dprintf(out, _("Cannot find plugin ID: %d\n"), id);
		}
		else
		{
			r = plugin_registry_unload(out, id);
		}
		exclusive_section_leave();
		return r;
	}
	if(!strcmp(argv[0], "rcon-get"))
	{
		int r = 0;
		struct rcon_host_connarg *connarg = rcon_host_getconnarg();
		if(connarg == NULL)
		{
			dprintf(out, _("Rcon is disabled.\n"));
		}
		else
		{
			dprintf(out, _("Host:\t%s\nPort:\t%s\n"), connarg->host, connarg->port);
		}
		return r;
	}
	if(!strcmp(argv[0], "rcon-set"))
	{
		int r = 0;
		bool disable = false;
		if(argc != 4)
		{
			if(argc == 2 && !strcmp("disable", argv[1]))
			{
				disable = true;
			}
			else
			{
				dprintf(out, _("Usage: rcon-set <host> <port> <password>\n"));
				dprintf(out, _("Usage: rcon-set disable\n"));
				return 64;
			}
		}
		// Always allocate a new one to make sure it is atomic.
		struct rcon_host_connarg *newargs = NULL;
		if(!disable)
		{
			newargs = malloc(sizeof(struct rcon_host_connarg));
			if(newargs == NULL)
			{
				r = errno;
				dprintf(out, _("Cannot allocate memory: %d.\n"), r);
				return r;
			}
			newargs->host = NULL;
			newargs->port = NULL;
			newargs->password = NULL;
			int size = 0;
			size = strlen(argv[1]) + 1;
			newargs->host = calloc(size, sizeof(char));
			if(newargs->host == NULL)
			{
				r = errno;
				dprintf(out, _("Cannot allocate memory: %d\n"), r);
				rcon_host_connarg_free(newargs);
				return r;
			}
			memcpy(newargs->host, argv[1], size);

			size = strlen(argv[2]) + 1;
			newargs->port = calloc(size, sizeof(char));
			if(newargs->port == NULL)
			{
				r = errno;
				dprintf(out, _("Cannot allocate memory: %d\n"), r);
				rcon_host_connarg_free(newargs);
				return r;
			}
			memcpy(newargs->port, argv[2], size);

			size = strlen(argv[3]) + 1;
			newargs->password = calloc(size, sizeof(char));
			if(newargs->password == NULL)
			{
				r = errno;
				dprintf(out, _("Cannot allocate memory: %d\n"), r);
				rcon_host_connarg_free(newargs);
				return r;
			}
			memcpy(newargs->password, argv[3], size);
		}

		struct rcon_host_connarg *connarg = rcon_host_getconnarg();
		rcon_host_setconnarg(newargs);
		if(connarg != NULL) rcon_host_connarg_free(connarg);
		dprintf(out, _("Ongoing requests will not be cancelled. Existing connections will be updated when plugins make requests.\n"));
		return r;
	}
	dprintf(out, "Unexpected action: '%s'\n", argv[0]);
	return 64;
}

static void *main_ctlsocket(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	thread_set_name("ctl-socket");
	int r = 0;
	char buf[1025];
	while(true)
	{
		const int accept_fd = accept(ctl_fd, NULL, NULL);
		if(accept_fd == -1)
		{
			r = errno;
			strerror_r(r, buf, 128);
			fprintf(stderr, _("Cannot accept control connection: %s\n"), buf);
			goto cleanup;
		}
		const ssize_t size = read(accept_fd, buf, 1024);
		buf[size] = '\0';
		
		char *pch = NULL;
		char **argv = calloc(1, sizeof(char*));
		if(argv == NULL)
		{
			dprintf(accept_fd, _("Cannot allocate memory: %d.\n%d"), errno, errno);
			close(accept_fd);
			continue;
		}
		int argc = 0;
		pch = strtok(buf, " ");
		bool fail = false;
		while(pch != NULL)
		{
			if(pch[strlen(pch) - 1] == '\n') pch[strlen(pch) - 1] = '\0';
			argc ++;
			char **argv_ext = realloc(argv, argc * sizeof(char*));
			if(argv_ext == NULL)
			{
				dprintf(accept_fd, _("Cannot allocate memory: %d.\n%d"), errno, errno);
				free(argv);
				fail = true;
				close(accept_fd);
				break;
			}
			argv = argv_ext;
			argv[argc - 1] = pch;
			pch = strtok(NULL, " ");
		}
		if(fail) continue;
		int resp = main_handle_cmd(accept_fd, argc, argv);
		dprintf(accept_fd, "%d", resp);
		free(argv);
		close(accept_fd);
	}

	goto cleanup;
cleanup:
	pthread_exit(NULL);
	return NULL;
}

static int setup_sem()
{
	int r = sem_init(&exit_sem, 0, 0);
	if(r)
	{
		fprintf(stderr, "sem_init(): %s\n", strerror(errno));
		goto cleanup;
	}
	goto cleanup;
cleanup:
	return r;
}

static int setup_sock()
{
	int r = 0;
	ctl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(ctl_fd == -1)
	{
		r = errno;
		fprintf(stderr, _("Cannot create control socket: %s\n"), strerror(r));
		goto cleanup;
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 1);
	int unlink_r = unlink(CONTROL_SOCKET_PATH);
	if(unlink_r && errno != ENOENT)
	{
		r = errno;
		fprintf(stderr, _("unlink(): %s\n"), strerror(r));
		goto cleanup;
	}
	r = bind(ctl_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
	if(r)
	{
		r = errno;
		fprintf(stderr, _("Cannot bind to the control socket: %s\n"), strerror(r));
		goto cleanup;
	}
	r = listen(ctl_fd, 5);
	if(r)
	{
		r = errno;
		fprintf(stderr, _("Cannot listen to the control socket: %s\n"), strerror(r));
		goto cleanup;
	}
	goto cleanup;
cleanup:
	return r;
}

int setup_sigmask(sigset_t *set)
{
	sigemptyset(set);
	sigaddset(set, SIGPIPE);
	sigaddset(set, SIGTERM);
	sigaddset(set, SIGINT);
	int r = pthread_sigmask(SIG_BLOCK, set, NULL);
	if(r)
	{
		fprintf(stderr, _("pthread_sigmask(): %d\n"), r);
		goto cleanup;
	}
	goto cleanup;
cleanup:
	return r;
}

static int setup_thread(pthread_t *thread, void *start_routine, void *arg)
{
	int r = pthread_create(thread, NULL, start_routine, arg);
	if(r)
	{
		fprintf(stderr, _("Cannot setup thread: %d\n"), r);
		goto cleanup;
	}
	goto cleanup;
cleanup:
	return r;
}

static int destroy_thread(pthread_t thread)
{
	int r = 0;
	r = pthread_cancel(thread);
	if(r && r != ESRCH)
	{
		fprintf(stderr, "pthread_cancel: %d.\n", r);
	}
	else
	{
		r = 0;
	}
	r = pthread_join(thread, NULL);
	if(r && r != ESRCH)
	{
		fprintf(stderr, "pthread_join: %d.\n", r);
	}
	else
	{
		r = 0;
	}
	return r;
}

static int main_daemon(int argc, char **argv)
{
	DEBUG("main.c#main_daemon: main_daemon()\n");
	bool sem_setup = false,
	     mcin_setup = false,
	     rcon_setup = false,
	     reg_setup = false,
	     sock_setup = false,
	     autoload_setup = false,
	     sigmask_setup = false,
	     thpool_setup = false,
	     sighandler_setup = false,
	     loop_setup = false,
	     socket_thread_setup = false;

	int r = 0;

	DEBUG("main.c#main_daemon: Setup semaphore...\n");
	r = setup_sem();
	if(r) goto cleanup;
	else sem_setup = true;

	DEBUG("main.c#main_daemon: Setup regular expressions...\n");
	r = mcin_init();
	if(r) goto cleanup;
	else mcin_setup = true;

	DEBUG("main.c#main_daemon: Setup rcon host...\n");
	r = rcon_host_init();
	if(r) goto cleanup;
	else rcon_setup = true;
	
	DEBUG("main.c#main_daemon: Loading pre-defined rcon arguments from environment variables.\n");
	const char *connarg_env_host = getenv("RCON_HOST");
	const char *connarg_env_port = getenv("RCON_PORT");
	const char *connarg_env_password = getenv("RCON_PASSWORD");
	if(!(connarg_env_host == NULL && connarg_env_port == NULL && connarg_env_password == NULL))
	{
		if(connarg_env_host != NULL && connarg_env_port != NULL && connarg_env_password != NULL)
		{
			struct rcon_host_connarg *connarg = malloc(sizeof(struct rcon_host_connarg));
			int size = 0;
			size = strlen(connarg_env_host) + 1;
			connarg->host = calloc(size, sizeof(char));
			memcpy(connarg->host, connarg_env_host, size);
			size = strlen(connarg_env_port) + 1;
			connarg->port = calloc(size, sizeof(char));
			memcpy(connarg->port, connarg_env_port, size);
			size = strlen(connarg_env_password) + 1;
			connarg->password = calloc(size, sizeof(char));
			memcpy(connarg->password, connarg_env_password, size);
			rcon_host_setconnarg(connarg);
			DEBUGF("main.c#main_daemon: Loaded rcon arguments from environment variables:\nHost:\t%s\nPort:\t%s\nPassword:\t%s\n",
					connarg->host,
					connarg->port,
					connarg->password);
		}
		else
		{
			fprintf(stderr, _("Cannot load rcon settings: RCON_HOST, RCON_PORT and RCON_PASSWORD must all present.\n"));
			r = 64;
			goto cleanup;
		}
	}

	DEBUG("main.c#main_daemon: Setup plugin registry...\n");
	r = plugin_registry_init();
	if(r) goto cleanup;
	else reg_setup = true;

	DEBUG("main.c#main_daemon: Setup control socket...\n");
	r = setup_sock();
	if(r) goto cleanup;
	else sock_setup = true;

	DEBUG("main.c#main_daemon: Setup signal masks...\n");
	sigset_t set;
	r = setup_sigmask(&set);
	if(r) goto cleanup;
	else sigmask_setup = true;

	DEBUG("main.c#main_daemon: Setup thread pool...\n");
	int thpool_threads = 1;
	if(getenv("THPOOL_THREADS") != NULL)
	{
		char *endptr;
		uintmax_t num = strtoumax(getenv("THPOOL_THREADS"), &endptr, 10);
		if(strcmp(endptr, "") || (num == UINTMAX_MAX && errno == ERANGE) || num > INT_MAX || num <= 0)
		{
			fprintf(stderr, _("Invalid THPOOL_THREADS value.\n"));
			r = 64;
			goto cleanup;
		}
		thpool_threads = (int)num;
	}
	DEBUGF("main.c#main_daemon: Using '%d' threads.\n", thpool_threads);
	thpool = thpool_init(thpool_threads);
	thpool_setup = true;

	if(argc > 1)
	{
		DEBUG("main.c#main_daemon: Autoloading plugins at startup...\n");
		r = autoload(argv[1]);
		if(r) goto cleanup;
		else autoload_setup = true;
	}

	DEBUG("main.c#main_daemon: Setup signal handler thread...\n");
	pthread_t thread_sighandler;
	r = setup_thread(&thread_sighandler, &main_sighandler, &set);
	if(r) goto cleanup;
	else sighandler_setup = true;

	DEBUG("main.c#main_daemon: Setup main loop thread...\n");
	pthread_t thread_loop;
	r = setup_thread(&thread_loop, &main_loop, NULL);
	if(r) goto cleanup;
	else loop_setup = true;

	DEBUG("main.c#main_daemon: Setup control socket thread...\n");
	pthread_t thread_ctlsocket;
	r = setup_thread(&thread_ctlsocket, &main_ctlsocket, NULL);
	if(r) goto cleanup;
	else socket_thread_setup = true;
	
	// Setup done. Enter blocking.

	DEBUG("main.c#main_daemon: Main: Setup done. Waiting.\n");
	r = sem_wait(&exit_sem);
	if(r)
	{
		fprintf(stderr, "sem_wait(): %d.\n", r);
		goto cleanup;
	}

	goto cleanup;
cleanup:
	DEBUG("main.c#main_daemon: Cleanup semaphore...\n");
	if(sem_setup) sem_destroy(&exit_sem);
	DEBUG("main.c#main_daemon: Cleanup regular expressions...\n");
	if(mcin_setup) mcin_free();
	DEBUG("main.c#main_daemon: Cleanup control socket thread...\n");
	if(socket_thread_setup) destroy_thread(thread_ctlsocket);
	DEBUG("main.c#main_daemon: Cleanup control socket...\n");
	if(sock_setup)
	{
		close(ctl_fd);
		unlink(CONTROL_SOCKET_PATH);
	}
	DEBUG("main.c#main_daemon: Cleanup loop thread...\n");
	if(loop_setup) destroy_thread(thread_loop);
	DEBUG("main.c#main_daemon: Cleanup signal handler thread...\n");
	if(sighandler_setup) destroy_thread(thread_sighandler);
	// Always perform thpool_wait after the main loop thread is paused or stopped.
	DEBUG("main.c#main_daemon: Cleanup thread pool...\n");
	if(thpool_setup)
	{
		thpool_wait(thpool);
		thpool_destroy(thpool);
	}
	if(autoload_setup) {} // Plugins are always unloaded.
	DEBUG("main.c#main_daemon: Unloading plugins...\n");
	const int size = plugin_size();
	int *plugins = calloc(size, sizeof(int));
	for(int i = 0; i < size; i ++)
		plugins[i] = plugin_get_by_index(i)->id;
	for(int i = 0; i < size; i ++)
	{
		int unload_r = plugin_registry_unload(2, plugins[i]);
		if(unload_r)
		{
			fprintf(stderr, _("Unload: %d\n"), unload_r);
		}
	}
	free(plugins);
	DEBUG("main.c#main_daemon: Cleanup rcon host...\n");
	struct rcon_host_connarg *connarg = rcon_host_getconnarg();
	if(connarg != NULL) rcon_host_connarg_free(connarg);
	if(rcon_setup) { rcon_host_free(); }
	DEBUG("main.c#main_daemon: Cleanup plugin registry...\n");
	if(reg_setup) plugin_registry_free();
	// Make the compiler happy: we don't need to do any cleanup for these items.
	if(sigmask_setup) {}
	return r;
}

static int main_ctl(int argc, char **argv)
{
	int r = 0;
	struct sockaddr_un addr;
	const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd == -1) {
		r = errno;
		fprintf(stderr, "%s\n", strerror(r));
		goto cleanup;
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, CONTROL_SOCKET_PATH, sizeof(addr.sun_path) - 1);
	r = connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if(r == -1)
	{
		r = errno;
		fprintf(stderr, "%s\n", strerror(r));
		goto cleanup;
	}
	unsigned int length = 0;
	for(int i = 1; i < argc; i ++)
	{
		length += strlen(argv[i]);
		if(i != argc - 1) length ++;
	}
	char *dat = calloc(length, sizeof(char));
	strcpy(dat, "");
	for(int i = 1; i < argc; i ++)
	{
		strcat(dat, argv[i]);
		if(i != argc - 1) strcat(dat, " ");
	}
	dprintf(fd, "%s", dat);

	ssize_t num_read;
	int arr_size = 1025;
	char *buffer = calloc(1025, sizeof(char));
	strcpy(buffer, "");
	char buf[1025];
	while((num_read = read(fd, buf, 1024)) > 0)
	{
		buf[num_read] = '\0';
		arr_size += 1024;
		buffer = realloc(buffer, arr_size * sizeof(char));
		strcat(buffer, buf);
	}
	const char *last_newline = strrchr(buffer, '\n');
	char *endptr;
	// Try parse if the whole thing is an exit code.
	if(last_newline == NULL)
	{
        	intmax_t num = strtoimax(buffer, &endptr, 10);
		if(!strcmp(endptr, "") && !(num == INTMAX_MAX && errno == ERANGE) && num <= INT_MAX && num >= INT_MIN)
		{
			r = (int)num;
			buffer[0] = '\0';
		}
	}
	else
	{
	        intmax_t num = strtoimax(&buffer[(int)(last_newline - buffer) + 1], &endptr, 10);
	        if(strcmp(endptr, "") || (num == INTMAX_MAX && errno == ERANGE) || num > INT_MAX || num < INT_MIN)
		{
			r = 255;
		}
		else
		{
			r = (int)num;
			buffer[(int)(last_newline - buffer) + 1] = '\0';
		}
	}
	printf("%s", buffer);
	free(buffer);

cleanup:
	if(fd != -1)
		close(fd);
	return r;
}

int main(int argc, char **argv)
{
	bool invoke_as_ctl = false;
	if(argc > 0)
	{
		char *path = strrchr(argv[0], '/');
		if(path != NULL)
		{
			const char *substr = &argv[0][(int)(path - argv[0]) + 1];
			invoke_as_ctl = !strcmp(substr, "extmcctl");
		}
	}
	if(invoke_as_ctl)
	{
		return main_ctl(argc, argv);
	}
	else
	{
		return main_daemon(argc, argv);
	}
}
