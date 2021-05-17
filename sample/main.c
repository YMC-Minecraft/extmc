/*
 * Copyright 2019 ~ 2021 YuutaW Minecraft, All Rights Reserved.
 * Proprietary and confidential.
 * Unauthorized copying of any parts of this file, via any medium is strictly prohibited.
 * Written by Yuuta Liang <yuuta@yuuta.moe>, April 2021.
 */

#include "../plugin/plugin.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

const uint32_t epg_version = 1;

const char *epg_name = "Test Plugin";

int epg_load(struct epg_handle *handle)
{
	printf("[%d]: Loaded.\n", handle->id);
	int r = 0;
	r = handle->rcon_send(11, "list uuid");
	printf("[%d]: rcon_send: %d\n", handle->id, r);
	int id = -1;
	char out[RCON_DATA_BUFFSIZE];
	r = handle->rcon_recv(&id, out);
	printf("[%d]: rcon_recv: %d. ID: %d, out: '%s'.\n", handle->id, r, id, out);
	return 0;
}

int epg_unload(struct epg_handle *handle)
{
	printf("[%d]: Unloading.\n", handle->id);
	return 0;
}

int epg_player_join(struct epg_handle *handle,
		char *player)
{
	printf("[%d]: %s joined.\n", handle->id, player);
	sleep(10);
	int r = 0;
	r = handle->rcon_send(11, "list");
	printf("[%d]: rcon_send: %d\n", handle->id, r);
	int id = -1;
	char out[RCON_DATA_BUFFSIZE];
	r = handle->rcon_recv(&id, out);
	printf("[%d]: rcon_recv: %d. ID: %d, out: '%s'.\n", handle->id, r, id, out);
	return 0;
}

int epg_player_leave(struct epg_handle *handle,
		char *player,
		char *reason)
{
	printf("[%d]: %s left: %s.\n", handle->id, player, reason);
	return 0;
}

int epg_player_say(struct epg_handle *handle,
		char *player,
		char *content)
{
	printf("[%d]: %s said: %s.\n", handle->id, player, content);
	return 0;
}

int epg_player_die(struct epg_handle *handle,
		char *player,
		char *source)
{
	printf("[%d]: %s died because of %s.\n", handle->id, player, source);
	return 0;
}

int epg_player_achievement(struct epg_handle *handle,
		char *player,
		char *achievement)
{
	printf("[%d]: %s achieved %s.\n", handle->id, player, achievement);
	return 0;
}

int epg_player_challenge(struct epg_handle *handle,
		char *player,
		char *challenge)
{
	printf("[%d]: %s made the challenge: %s.\n", handle->id, player, challenge);
	return 0;
}

int epg_player_goal(struct epg_handle *handle,
		char *player,
		char *goal)
{
	printf("[%d]: %s made the goal: %s.\n", handle->id, player, goal);
	return 0;
}

int epg_server_stopping(struct epg_handle *handle)
{
	printf("[%d]: Server stopped.\n", handle->id);
	return 0;
}

int epg_server_starting(struct epg_handle *handle,
		char *version)
{
	printf("[%d]: Server is starting, version: %s.\n", handle->id, version);
	return 0;
}

int epg_server_started(struct epg_handle *handle,
		char *took)
{
	printf("[%d]: Server started, took: %s.\n", handle->id, took);
	return 0;
}
