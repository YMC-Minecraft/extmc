#include "plugins.h"
#include "common.h"
#include "plugin_registry.h"

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <string.h>

static const void *plugin_dlsym(int stderr_fd, void *handle, const bool mandatory, const char *name)
{
	const void *sym = dlsym(handle, name);
	if(sym == NULL)
	{
		if(mandatory)
		{
			dprintf(stderr_fd, _("Cannot load %s, aborting: %s.\n"), name, dlerror());
		}
		else
		{
			dprintf(stderr_fd, _("Cannot load %s, ignoring: %s.\n"), name, dlerror());
		}
		return NULL;
	}
	return sym;
}

int plugin_unload(int stderr_fd, struct plugin *plugin)
{
	if(plugin->fc_unload != NULL)
	{
		struct epg_handle hdl;
		plugcall_setup_handle(plugin, &hdl);
		int unload_r = plugin->fc_unload(&hdl);
		if(unload_r)
		{
			dprintf(stderr_fd, _("Cannot unload plugin: it returned an error: %d.\n"), unload_r);
			return unload_r;
		}
	}
	if(plugin->handle != NULL)
	{
		dlclose(plugin->handle);
		plugin->handle = NULL;
	}
	return 0;
}

static int plugin_load_v1(int stderr_fd, struct plugin *out)
{
	int r = 0;
	const void *sym;
	sym = plugin_dlsym(stderr_fd, out->handle, true, "epg_name");
	if(sym == NULL)
	{
		r = 64;
		goto cleanup;
	}
	out->name = *(char**)sym;
	out->fc_load = plugin_dlsym(stderr_fd, out->handle, false, "epg_load");
	out->fc_unload = plugin_dlsym(stderr_fd, out->handle, false, "epg_unload");
	out->fc_player_join = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_join");
	out->fc_player_leave = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_leave");
	out->fc_player_say = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_say");
	out->fc_player_die = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_die");
	out->fc_player_achievement = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_achievement");
	out->fc_player_challenge = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_challenge");
	out->fc_player_goal = plugin_dlsym(stderr_fd, out->handle, false, "epg_player_goal");
	out->fc_server_stopping = plugin_dlsym(stderr_fd, out->handle, false, "epg_server_stopping");
	out->fc_server_starting = plugin_dlsym(stderr_fd, out->handle, false, "epg_server_starting");
	out->fc_server_started = plugin_dlsym(stderr_fd, out->handle, false, "epg_server_started");
	goto cleanup;
cleanup:
	return r;
}

int plugin_load(int stderr_fd, const char *path, const int id, struct plugin *out)
{
	int r = 0;
	out->id = id;
	out->path = path;
	out->handle = NULL;
	out->name = NULL;
	out->version = 0;
	out->fc_load = NULL;
	out->fc_unload = NULL;
	out->fc_player_join = NULL;
	out->fc_player_leave = NULL;
	out->fc_player_say = NULL;
	out->fc_player_die = NULL;
	out->fc_player_achievement = NULL;
	out->fc_player_challenge = NULL;
	out->fc_player_goal = NULL;
	out->fc_server_stopping = NULL;
	out->fc_server_starting = NULL;
	out->fc_server_started = NULL;

	void *handle = dlopen(path, RTLD_LAZY);
	if(handle == NULL)
	{
		dprintf(stderr_fd, _("Cannot load %s: %s.\n"), path, dlerror());
		r = 1;
		goto cleanup;
	}
	out->handle = handle;
	const void *sym = plugin_dlsym(stderr_fd, handle, true, "epg_version");
	if(sym == NULL)
	{
		r = 64;
		goto cleanup;
	}
	out->version = *(uint32_t*)sym;
	switch(out->version)
	{
		case 1:
			r = plugin_load_v1(stderr_fd, out);
			if(r) goto cleanup;
			break;
		default:
			dprintf(stderr_fd, _("Unsupported plugin %s: Incompatible with version %u.\n"), path, out->version);
			break;
	}
	struct epg_handle hdl;
	plugcall_setup_handle(out, &hdl);
	r = out->fc_load(&hdl);
	if(r)
	{
		dprintf(stderr_fd, _("Cannot load plugin: it returned an error: %d.\n"), r);
		goto cleanup;
	}
	goto cleanup;
cleanup:
	if(r) plugin_unload(stderr_fd, out);
	return r;
}
