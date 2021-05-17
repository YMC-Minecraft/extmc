#ifndef _PLUGIN_REGISTRY_H
#define _PLUGIN_REGISTRY_H

#include "plugins.h"

#define EPLUGINEXCEED	10
#define EPLUGINNOTFOUND	74

int plugin_registry_init();
void plugin_registry_free();

int plugin_size();
struct plugin *plugin_get(int id);
struct plugin *plugin_get_by_index(int index);
int plugin_registry_unload(int stderr_fd, int id);
int plugin_registry_load(int stderr_fd, const char *path, int *id);

void plugcall_setup_handle(struct plugin *plugin, struct epg_handle *handle);

void plugcall_player_join(void *arg);
void plugcall_player_leave(void *arg);
void plugcall_player_achievement(void *arg);
void plugcall_player_challenge(void *arg);
void plugcall_player_goal(void *arg);
void plugcall_player_say(void *arg);
void plugcall_player_die(void *arg);
void plugcall_server_stopping(void *arg);
void plugcall_server_starting(void *arg);
void plugcall_server_started(void *arg);

#endif // _PLUGIN_REGISTRY_H
