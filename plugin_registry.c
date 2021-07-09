#include "plugins.h"
#include "plugin_registry.h"
#include "rcon_host.h"
#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define PLUGIN_ID_GEN_MAX_RETRY 1

static int plugin_count = 0;
/* TODO: May use hashing to improve search preformance */
/* TODO: May use sorting + binary search to improve search performance */
static const char **id_arr = NULL;
static struct plugin *plugin_arr = NULL;
static pthread_key_t key_plugin;

static void arr_resize(int new_size)
{
	if(new_size == 0)
	{
		if(id_arr != NULL)
		{
			free(id_arr);
			id_arr = NULL;
		}
		if(plugin_arr != NULL)
		{
			free(plugin_arr);
			plugin_arr = NULL;
		}
		return;
	}
	if(id_arr == NULL)
		id_arr = calloc(new_size, sizeof(char *));
	else
		id_arr = realloc(id_arr, new_size * sizeof(char *));
	if(plugin_arr == NULL)
		plugin_arr = calloc(new_size, sizeof(struct plugin));
	else
		plugin_arr = realloc(plugin_arr, new_size * sizeof(struct plugin));
}

int plugin_registry_init()
{
	pthread_key_create(&key_plugin, NULL);
	return 0;
}

void plugin_registry_free()
{
	plugin_count = 0;
	arr_resize(0);
	pthread_key_delete(key_plugin);
}

int plugin_size()
{
	return plugin_count;
}

static int plugin_id_to_index(const char *id)
{
	for(int i = 0; i < plugin_count; i ++)
	{
		if(!strcmp(id_arr[i], id))
			return i;
	}
	return -1;
}

struct plugin *plugin_get(const char *id)
{
	const int index = plugin_id_to_index(id);
	if(index < 0)
		return NULL;
	return &plugin_arr[index];
}

struct plugin *plugin_get_by_index(int index)
{
	return &plugin_arr[index];
}

int plugin_registry_unload(int stderr_fd, const char *id)
{
	int r = 0;
	const int index = plugin_id_to_index(id);
	if(index < 0)
	{
		r = EPLUGINNOTFOUND;
		goto cleanup;
	}
	struct plugin *plug = plugin_get_by_index(index);
	r = plugin_unload(stderr_fd, plug);
	if(r) goto cleanup;
	r = plugin_unload_meta(stderr_fd, plug);
	if(r) goto cleanup;
	memcpy(plug, &plugin_arr[index + 1], (plugin_count - 1 - index) * sizeof(struct plugin));
	memcpy((char **)id_arr[index], &id_arr[index + 1], (plugin_count - 1 - index) * sizeof(char *));
	arr_resize(-- plugin_count);
	goto cleanup;
cleanup:
	return r;
}

int plugin_registry_load(int stderr_fd, const char *path)
{
	int r = 0;
	struct plugin plugin;
	r = plugin_load_meta(stderr_fd, path, &plugin);
	if(r) goto cleanup;
	if(plugin_get(plugin.id) != NULL)
	{
		dprintf(stderr_fd, _("Plugin '%s' exists.\n"), plugin.id);
		plugin_unload_meta(stderr_fd, &plugin);
		r = EPLUGINEXISTS;
		goto cleanup;
	}
	r = plugin_load(stderr_fd, &plugin);
	if(r)
	{
		plugin_unload_meta(stderr_fd, &plugin);
		goto cleanup;
	}
	arr_resize(++ plugin_count);
	id_arr[plugin_count - 1] = plugin.id;
	memcpy(&plugin_arr[plugin_count - 1], &plugin, sizeof(struct plugin));
	goto cleanup;
cleanup:
	return r;
}

static int api_rcon_send_wrapper(int pkt_id, char *command)
{
	int r = 0;
	const struct plugin *plug = pthread_getspecific(key_plugin);
	printf(_("[rcon#%s] -> '%s' (%d)\n"),
			plug->id,
			command,
			pkt_id);
	// TODO: The plugin identity may have future usages.
	r = rcon_host_send(pkt_id, command);
	if(r) goto cleanup;
cleanup:
	return r;
}

static int api_rcon_recv_wrapper(int *pkt_id, char *out)
{
	int r = 0;
	const struct plugin *plug = pthread_getspecific(key_plugin);
	// TODO: The plugin identity may have future usages.
	r = rcon_host_recv(pkt_id, out);
	if(!r)
		printf(_("[rcon#%s] <- %s (%d)\n"),
				plug->id,
				out,
				*pkt_id);
	if(r) goto cleanup;
cleanup:
	return r;
}

void plugcall_setup_handle(const struct plugin *plugin, struct epg_handle *handle)
{
	pthread_setspecific(key_plugin, plugin);
	handle->id = plugin->id;
	handle->rcon_send = &api_rcon_send_wrapper;
	handle->rcon_recv = &api_rcon_recv_wrapper;
}

#define PLUGCALL_PRE(X) \
	struct epg_handle handle; \
	struct plugin_call_job_args *args = arg; \
	struct plugin *plugin = plugin_get_by_index(args->id); \
	plugcall_setup_handle(plugin, &handle);

#define PLUGCALL_POST(X) \
	if(args->arg1 != NULL) free(args->arg1); \
	if(args->arg2 != NULL) free(args->arg2); \
	if(args->arg3 != NULL) free(args->arg3); \
	if(args->arg4 != NULL) free(args->arg4); \
	if(args->arg5 != NULL) free(args->arg5); \
	free(args);

void plugcall_player_join(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_join(&handle, args->arg1);
	PLUGCALL_POST(arg)
}

void plugcall_player_leave(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_leave(&handle, args->arg1, args->arg2);
	PLUGCALL_POST(arg)
}

void plugcall_player_achievement(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_achievement(&handle, args->arg1, args->arg2);
	PLUGCALL_POST(arg)
}

void plugcall_player_challenge(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_challenge(&handle, args->arg1, args->arg2);
	PLUGCALL_POST(arg)
}

void plugcall_player_goal(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_goal(&handle, args->arg1, args->arg2);
	PLUGCALL_POST(arg)
}

void plugcall_player_say(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_say(&handle, args->arg1, args->arg2);
	PLUGCALL_POST(arg)
}

void plugcall_player_die(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_player_die(&handle, args->arg1, args->arg2);
	PLUGCALL_POST(arg)
}

void plugcall_server_stopping(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_server_stopping(&handle);
	PLUGCALL_POST(arg)
}

void plugcall_server_starting(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_server_starting(&handle, args->arg1);
	PLUGCALL_POST(arg)
}

void plugcall_server_started(void *arg)
{
	PLUGCALL_PRE(arg)
	plugin->fc_server_started(&handle, args->arg1);
	PLUGCALL_POST(arg)
}
