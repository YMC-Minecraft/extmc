/*
 * Copyright 2019 ~ 2021 YuutaW Minecraft, All Rights Reserved.
 * Proprietary and confidential.
 * Unauthorized copying of any parts of this file, via any medium is strictly prohibited.
 * Written by Yuuta Liang <yuuta@yuuta.moe>, April 2021.
 */

#ifndef _PLUGINS_H
#define _PLUGINS_H

#include "plugin/plugin.h"

struct plugin_call_job_args {
	int id;
	char *arg1;
	char *arg2;
	char *arg3;
	char *arg4;
	char *arg5;
};

struct plugin {
	int id;
	const char *path;
	void *handle;
	char *name;
	uint32_t version;
	int (*fc_load)(struct epg_handle *);
	int (*fc_unload)(struct epg_handle *);
	int (*fc_player_join)(struct epg_handle *, char *);
	int (*fc_player_leave)(struct epg_handle *, char *, char *);
	int (*fc_player_say)(struct epg_handle *, char *, char *);
	int (*fc_player_die)(struct epg_handle *, char *, char *);
	int (*fc_player_achievement)(struct epg_handle *, char *, char *);
	int (*fc_player_challenge)(struct epg_handle *, char *, char *);
	int (*fc_player_goal)(struct epg_handle *, char *, char *);
	int (*fc_server_stopping)(struct epg_handle *);
	int (*fc_server_starting)(struct epg_handle *, char *);
	int (*fc_server_started)(struct epg_handle *, char *);
};

int plugin_load(int stderr_fd, const char *path, const int id, struct plugin *out);
int plugin_unload(int stderr_fd, struct plugin *plugin);

#endif // _PLUGINS_H
