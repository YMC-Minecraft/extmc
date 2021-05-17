#include "mcin.h"
#include "common.h"
#include "thpool.h"
#include "plugins.h"
#include "plugin_registry.h"

#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static regex_t reg_master;

static regex_t reg_player_join;
static regex_t reg_player_leave;
static regex_t reg_player_achievement;
static regex_t reg_player_challenge;
static regex_t reg_player_goal;
static regex_t reg_player_say;

static regex_t reg_player_die[64];

static regex_t reg_server_stopping;
static regex_t reg_server_starting;
static regex_t reg_server_started;

static int mcin_compile_regex(const char *regex, const int flags, regex_t *reg)
{
	int r = regcomp(reg, regex, flags);
	if(r)
	{
		fprintf(stderr, _("Cannot compile regex: %s.\n"), regex);
	}
	return r;
}

#define mcin_compile_death_msg(index, regex) \
{ \
	r = mcin_compile_regex(regex, REG_EXTENDED, &reg_player_die[index]); \
	if(r) return r; \
}

int mcin_init()
{
	int r = 0;
	r = mcin_compile_regex(".*\\[([0-9][0-9]:[0-9][0-9]:[0-9][0-9])\\] \\[(.*)\\/(.*)\\]: (.*)\n", REG_EXTENDED, &reg_master);
	if(r) return r;
	r = mcin_compile_regex("(.*) joined the game", REG_EXTENDED, &reg_player_join);
	if(r) return r;
	r = mcin_compile_regex("(.*) lost connection: (.*)", REG_EXTENDED, &reg_player_leave);
	if(r) return r;
	r = mcin_compile_regex("(.*) has made the advancement \\[(.*)\\]", REG_EXTENDED, &reg_player_achievement);
	if(r) return r;
	r = mcin_compile_regex("(.*) has completed the challenge \\[(.*)\\]", REG_EXTENDED, &reg_player_challenge);
	if(r) return r;
	r = mcin_compile_regex("(.*) has reached the goal \\[(.*)\\]", REG_EXTENDED, &reg_player_goal);
	if(r) return r;
	r = mcin_compile_regex("<(.*)> (.*)", REG_EXTENDED, &reg_player_say);
	if(r) return r;

	r = mcin_compile_regex("Stopping server", REG_EXTENDED, &reg_server_stopping);
	if(r) return r;
	r = mcin_compile_regex("Starting minecraft server version (.*)", REG_EXTENDED, &reg_server_starting);
	if(r) return r;
	r = mcin_compile_regex("Done \\((.*s)\\)! For help, type \"help\"", REG_EXTENDED, &reg_server_started);
	if(r) return r;

	mcin_compile_death_msg(0, "(.*) was shot by (.*)");
	mcin_compile_death_msg(1, "(.*) was shot by (.*) using .*");
	mcin_compile_death_msg(2, "(.*) was pummeled by (.*)");
	mcin_compile_death_msg(3, "(.*) was pummeled by (.*) using .*");
	mcin_compile_death_msg(4, "(.*) was pricked to death");
	mcin_compile_death_msg(5, "(.*) walked into a cactus whilst trying to escape (.*)");
	mcin_compile_death_msg(6, "(.*) drowned");
	mcin_compile_death_msg(7, "(.*) drowned whilst trying to escape (.*)");
	mcin_compile_death_msg(8, "(.*) experienced kinetic energy");
	mcin_compile_death_msg(9, "(.*) experienced kinetic energy whilst trying to escape (.*)");
	mcin_compile_death_msg(10, "(.*) was blown up by (.*)");
	mcin_compile_death_msg(11, "(.*) was blown up by (.*) using .*");
	mcin_compile_death_msg(12, "(.*) was killed by \\[Intentional Game Design\\]");
	mcin_compile_death_msg(13, "(.*) hit the ground too hard");
	mcin_compile_death_msg(14, "(.*) hit the ground too hard whilst trying to escape (.*)");
	mcin_compile_death_msg(15, "(.*) fell from a high place");
	mcin_compile_death_msg(16, "(.*) fell off a ladder");
	mcin_compile_death_msg(17, "(.*) fell off some vines");
	mcin_compile_death_msg(18, "(.*) fell off some weeping vines");
	mcin_compile_death_msg(19, "(.*) fell off some twisting vines");
	mcin_compile_death_msg(20, "(.*) fell off scaffolding");
	mcin_compile_death_msg(21, "(.*) fell off while climbing");
	mcin_compile_death_msg(22, "(.*) was squashed by a falling anvil");
	mcin_compile_death_msg(23, "(.*) was squashed by a falling anvil whilst fighting (.*)");
	mcin_compile_death_msg(24, "(.*) was squashed by a falling block");
	mcin_compile_death_msg(25, "(.*) was squashed by a falling block whilst fighting (.*)");
	mcin_compile_death_msg(26, "(.*) went up in flames");
	mcin_compile_death_msg(27, "(.*) walked into fire whilst fighting (.*.)");
	mcin_compile_death_msg(28, "(.*) burned to death");
	mcin_compile_death_msg(29, "(.*) was burnt to a crisp whilst fighting (.*)");
	mcin_compile_death_msg(30, "(.*) went off with a bang");
	mcin_compile_death_msg(31, "(.*) went off with a bang due to a firework fired from .* by (.*)");
	mcin_compile_death_msg(32, "(.*) tried to swim in lava");
	mcin_compile_death_msg(33, "(.*) tried to swim in lava to escape (.*)");
	mcin_compile_death_msg(34, "(.*) was struck by lightning");
	mcin_compile_death_msg(35, "(.*) was struck by lightning whilst fighting (.*)");
	mcin_compile_death_msg(36, "(.*) discovered the floor was lava");
	mcin_compile_death_msg(37, "(.*) walked into danger zone due to (.*)");
	mcin_compile_death_msg(38, "(.*) was killed by magic");
	mcin_compile_death_msg(39, "(.*) was killed by magic whilst trying to escape (.*)");
	mcin_compile_death_msg(40, "(.*) was killed by (.*) using magic");
	mcin_compile_death_msg(41, "(.*) was killed by (.*) using .*");
	mcin_compile_death_msg(42, "(.*) was slain by (.*)");
	mcin_compile_death_msg(43, "(.*) was slain by (.*) using .*");
	mcin_compile_death_msg(44, "(.*) was fireballed by (.*)");
	mcin_compile_death_msg(45, "(.*) was fireballed by (.*) using .*");
	mcin_compile_death_msg(46, "(.*) was stung to death");
	mcin_compile_death_msg(47, "(.*) was shot by a skull from (.*)");
	mcin_compile_death_msg(48, "(.*) starved to death");
	mcin_compile_death_msg(49, "(.*) starved to death whilst fighting (.*)");
	mcin_compile_death_msg(50, "(.*) suffocated in a wall");
	mcin_compile_death_msg(51, "(.*) suffocated in a wall whilst fighting (.*)");
	mcin_compile_death_msg(52, "(.*) was squished too much");
	mcin_compile_death_msg(53, "(.*) was squished by (.*)");
	mcin_compile_death_msg(54, "(.*) was poked to death by a sweet berry bush");
	mcin_compile_death_msg(55, "(.*) was poked to death by a sweet berry bush whilst trying to escape (.*)");
	mcin_compile_death_msg(56, "(.*) was killed trying to hurt (.*)");
	mcin_compile_death_msg(57, "(.*) was killed by .* trying to hurt (.*)");
	mcin_compile_death_msg(58, "(.*) was impaled by (.*)");
	mcin_compile_death_msg(59, "(.*) was impaled by (.*) with .*");
	mcin_compile_death_msg(60, "(.*) fell out of the world");
	mcin_compile_death_msg(61, "(.*) didn't want to live in the same world as (.*)");
	mcin_compile_death_msg(62, "(.*) withered away");
	mcin_compile_death_msg(63, "(.*) withered away whilst fighting (.*)");

	return r;
}

void mcin_free()
{
	regfree(&reg_master);
	regfree(&reg_player_join);
	regfree(&reg_player_leave);
	regfree(&reg_player_say);
	regfree(&reg_player_achievement);
	regfree(&reg_player_challenge);
	regfree(&reg_player_goal);
	for(int i = 0; i < 64; i ++)
		regfree(&reg_player_die[i]);
	regfree(&reg_server_stopping);
	regfree(&reg_server_starting);
	regfree(&reg_server_started);
}

static bool mcin_match_one_ex(const regex_t reg, const char *str, const int required_args, const int total_args, struct plugin_call_job_args *arg)
{
	regmatch_t pmatch[6];
	int r = regexec(&reg, str, 6, pmatch, 0);
	if(r)
	{
		return false;
	}
	if(pmatch[0].rm_so == -1) return false;
	arg->id = 0;
	arg->arg1 = NULL;
	arg->arg2 = NULL;
	arg->arg3 = NULL;
	arg->arg4 = NULL;
	arg->arg5 = NULL;
	for(int i = 1; i < total_args + 1; i ++)
	{
		const regmatch_t match = pmatch[i];
		if(match.rm_so == -1 && i < required_args)
		{
			// Not reaching the required number of arguments.
			if(arg->arg5 != NULL)
			{
				free(arg->arg5);
				arg->arg5 = NULL;
			}
			if(arg->arg4 != NULL)
			{
				free(arg->arg4);
				arg->arg4 = NULL;
			}
			if(arg->arg3 != NULL)
			{
				free(arg->arg3);
				arg->arg3 = NULL;
			}
			if(arg->arg2 != NULL)
			{
				free(arg->arg2);
				arg->arg2 = NULL;
			}
			if(arg->arg1 != NULL)
			{
				free(arg->arg1);
				arg->arg1 = NULL;
			}
			return false;
		}
		const int length = match.rm_eo - match.rm_so;
		char *substring = calloc(length + 1, sizeof(char));
		memcpy(substring, &str[match.rm_so], length);
		substring[length] = '\0';
		switch(i)
		{
			case 1:
				arg->arg1 = substring;
				break;
			case 2:
				arg->arg2 = substring;
				break;
			case 3:
				arg->arg3 = substring;
				break;
			case 4:
				arg->arg4 = substring;
				break;
			case 5:
				arg->arg5 = substring;
				break;
		}
	}
	return true;
}

static bool mcin_match_one(const regex_t reg, const char *str, const int required_args, struct plugin_call_job_args *arg)
{
	return mcin_match_one_ex(reg, str, required_args, required_args, arg);
}

static struct plugin_call_job_args *args_copy(const struct plugin_call_job_args *orig, int index)
{
	struct plugin_call_job_args *args = malloc(sizeof(struct plugin_call_job_args));
	args->id = index; // Index to ID resolution will happen in plugcall_*.
	if(orig->arg1 == NULL) args->arg1 = NULL;
	else
	{
		args->arg1 = calloc(strlen(orig->arg1) + 1, sizeof(char));
		strcpy(args->arg1, orig->arg1);
	}

	if(orig->arg2 == NULL) args->arg2 = NULL;
	else
	{
		args->arg2 = calloc(strlen(orig->arg2) + 1, sizeof(char));
		strcpy(args->arg2, orig->arg2);
	}
	if(orig->arg3 == NULL) args->arg3 = NULL;
	else
	{
		args->arg3 = calloc(strlen(orig->arg3) + 1, sizeof(char));
		strcpy(args->arg3, orig->arg3);
	}
	if(orig->arg4 == NULL) args->arg4 = NULL;
	else
	{
		args->arg4 = calloc(strlen(orig->arg4) + 1, sizeof(char));
		strcpy(args->arg4, orig->arg4);
	}
	if(orig->arg5 == NULL) args->arg5 = NULL;
	else
	{
		args->arg5 = calloc(strlen(orig->arg5) + 1, sizeof(char));
		strcpy(args->arg5, orig->arg5);
	}
	return args;
}

void mcin_match(const char *str, const threadpool thpool)
{
	struct plugin_call_job_args local;
	local.id = 0;
	local.arg1 = NULL;
	local.arg2 = NULL;
	local.arg3 = NULL;
	local.arg4 = NULL;
	local.arg5 = NULL;
	regmatch_t pmatch[5];
	if(regexec(&reg_master, str, 5, pmatch, 0)) return;
	char *temp_str = calloc(strlen(str) + 1, sizeof(char));
	for(int i = 1 /* Ignore the string itself */; i < 5; i ++)
	{
		const regmatch_t match = pmatch[i];
		if(match.rm_so == -1)
			// Shouldn't happen if the string is valid.
			goto cleanup;
		if(i != 2 && i != 3 && i != 4) continue; // We don't care.
		int length = match.rm_eo - match.rm_so;
		for(int j = 0; j < length; j ++)
		{
			temp_str[j] = str[match.rm_so + j];
		}
		temp_str[length] = '\0';
	
		if(i == 2) /* Tag */
			if(strcmp(temp_str, "Server thread"))
				goto cleanup;
		if(i == 3) /* Level */
			if(strcmp(temp_str, "INFO"))
				goto cleanup;
		if(i == 4) // Data
			break;
	}
	const int size = plugin_size();
	if(mcin_match_one(reg_player_join, temp_str, 1, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_player_join == NULL) continue;
			thpool_add_work(thpool, &plugcall_player_join, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_player_leave, temp_str, 2, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_player_leave == NULL) continue;
			thpool_add_work(thpool, &plugcall_player_leave, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_player_achievement, temp_str, 2, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_player_achievement == NULL) continue;
			thpool_add_work(thpool, &plugcall_player_achievement, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_player_challenge, temp_str, 2, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_player_challenge == NULL) continue;
			thpool_add_work(thpool, &plugcall_player_challenge, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_player_goal, temp_str, 2, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_player_goal == NULL) continue;
			thpool_add_work(thpool, &plugcall_player_goal, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_player_say, temp_str, 2, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_player_say == NULL) continue;
			thpool_add_work(thpool, &plugcall_player_say, args_copy(&local, i));
		}
		goto cleanup;
	}
	for(int i = 0; i < 64; i ++)
		if(mcin_match_one_ex(reg_player_die[i], temp_str, 1, 2, &local))
		{
			for(int j = 0; j < size; j ++)
			{
				if(plugin_get_by_index(i)->fc_player_die == NULL) continue;
				thpool_add_work(thpool, &plugcall_player_die, args_copy(&local, j));
			}
			goto cleanup;
		}
	if(mcin_match_one(reg_server_stopping, temp_str, 0, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_server_stopping == NULL) continue;
			thpool_add_work(thpool, &plugcall_server_stopping, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_server_starting, temp_str, 1, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_server_starting == NULL) continue;
			thpool_add_work(thpool, &plugcall_server_starting, args_copy(&local, i));
		}
		goto cleanup;
	}
	if(mcin_match_one(reg_server_started, temp_str, 1, &local))
	{
		for(int i = 0; i < size; i ++)
		{
			if(plugin_get_by_index(i)->fc_server_started == NULL) continue;
			thpool_add_work(thpool, &plugcall_server_started, args_copy(&local, i));
		}
		goto cleanup;
	}
	goto cleanup;
cleanup:
	if(local.arg5 != NULL)
	{
		free(local.arg5);
		local.arg5 = NULL;
	}
	if(local.arg4 != NULL)
	{
		free(local.arg4);
		local.arg4 = NULL;
	}
	if(local.arg3 != NULL)
	{
		free(local.arg3);
		local.arg3 = NULL;
	}
	if(local.arg2 != NULL)
	{
		free(local.arg2);
		local.arg2 = NULL;
	}
	if(local.arg1 != NULL)
	{
		free(local.arg1);
		local.arg1 = NULL;
	}
	free(temp_str);
}
