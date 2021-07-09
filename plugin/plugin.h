/*
 * Plugin API definition.
 */

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include "common.h"
#include <stdint.h>

/* When the administrator disabled rcon. The plugin must have a way to avoid using rcon since it is non-recoverable. */
#define EPG_RCON_DISABLED	-1

/* 32-bit unsigned integer to indicate the version of the API. */
extern const uint32_t epg_version;

/* Plugin display name. */
extern const char *epg_name;

/* NULL terminated unique ID */
extern const char *epg_id;

/* Current session handle. */
struct epg_handle {
	/* Unique ID. */
	const char *id;
	/* Send rcon command. */
	int (*rcon_send)(int, char *);
	int (*rcon_recv)(int *, char *);
};

/* Before the plugin is loaded.
 * Return a non-zero integer to indicate an error and the plugin will be unloaded immediatedly (without calling epg_unload).
 * Thread: main thread (during autoloading) or control socket (during extmcctl operations). */
int epg_load(struct epg_handle *handle);

/* Before the plugin is unloaded.
 * Return a non-zero integer to indicate an error and the plugin will not be unloaded.
 * Thread: main thread (during autoloading) or control socket (during extmcctl operations). */
int epg_unload(struct epg_handle *handle);

/*
 * When a player joins the game.
 * Thread: worker */
int epg_player_join(struct epg_handle *handle,
		char *player);

int epg_player_leave(struct epg_handle *handle,
		char *player,
		char *reason);

int epg_player_say(struct epg_handle *handle,
		char *player,
		char *content);

int epg_player_die(struct epg_handle *handle,
		char *player,
		char *source);

int epg_player_achievement(struct epg_handle *handle,
		char *player,
		char *challenge);

int epg_player_challenge(struct epg_handle *handle,
		char *player,
		char *challenge);

int epg_player_goal(struct epg_handle *handle,
		char *player,
		char *goal);

int epg_server_stopping(struct epg_handle *handle);

int epg_server_starting(struct epg_handle *handle,
		char *version);

int epg_server_started(struct epg_handle *handle,
		char *took);

#endif // _PLUGIN_H
