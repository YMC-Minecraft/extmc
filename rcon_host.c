#include "rcon_host.h"
#include "rcon.h"
#include "net.h"
#include "plugin/plugin.h"
#include "md5.h"

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include <errno.h>
#include <stdbool.h>

static bool pthread_key_init = false;
static pthread_key_t key_rcon_fd;

static struct rcon_host_connarg *connarg;
static uint64_t connarg_existing_hash_1;
static uint64_t connarg_existing_hash_2;

struct rcon_thread_data {
	int fd;
	uint64_t connarg_hash_1;
	uint64_t connarg_hash_2;
};

static void destructor(void *data)
{
	DEBUGF("rcon_host.c#destructor: (%p)\n", data);
	struct rcon_thread_data *dat = (struct rcon_thread_data *)data;
	if(dat->fd != -1)
	{
		DEBUGF("rcon_host.c#destructor: Closing rcon socket %d\n", dat->fd);
		close(dat->fd);
	}
	free(dat);
}

/* https://stackoverflow.com/a/25669375/6792243 */
static uint64_t uint8ArrtoUint64(uint8_t *var, uint32_t lowest_pos)
{
    return  (((uint64_t)var[lowest_pos+7]) << 56) | 
            (((uint64_t)var[lowest_pos+6]) << 48) |
            (((uint64_t)var[lowest_pos+5]) << 40) | 
            (((uint64_t)var[lowest_pos+4]) << 32) |
            (((uint64_t)var[lowest_pos+3]) << 24) | 
            (((uint64_t)var[lowest_pos+2]) << 16) |
            (((uint64_t)var[lowest_pos+1]) << 8)  | 
            (((uint64_t)var[lowest_pos])   << 0);
}

static void connarg_hash_update(struct rcon_host_connarg *connarg)
{
	uint64_t hash_1 = 0;
	uint64_t hash_2 = 0;
	if(connarg != NULL)
	{
		MD5Context ctx;
		md5Init(&ctx);
		if(connarg->host != NULL) md5Update(&ctx, (uint8_t *)connarg->host, sizeof(char) * strlen(connarg->host));
		if(connarg->port != NULL) md5Update(&ctx, (uint8_t *)connarg->port, sizeof(char) * strlen(connarg->port));
		if(connarg->password != NULL) md5Update(&ctx, (uint8_t *)connarg->password, sizeof(char) * strlen(connarg->password));
		md5Finalize(&ctx);
		hash_1 = uint8ArrtoUint64(ctx.digest, 0);
		hash_2 = uint8ArrtoUint64(ctx.digest, 8);
	}
	connarg_existing_hash_1 = hash_1;
	connarg_existing_hash_2 = hash_2;
	DEBUGF("rcon_host.c#connarg_hash: %lu%lu.\n", hash_1, hash_2);
}

static int rcon_host_clear_current_thread_socket()
{
	struct rcon_thread_data *data = pthread_getspecific(key_rcon_fd);
	if(data != NULL) data->fd = -1;
	return 0;
}

static int rcon_host_get_current_thread_socket(int *out)
{
	int r = 0;
	struct rcon_thread_data *data = pthread_getspecific(key_rcon_fd);
	if(data == NULL)
	{
		DEBUG("rcon_host.c#rcon_host_get_current_thread_socket: Allocating new thread specific data.\n");
		data = malloc(sizeof(struct rcon_thread_data));
		r = pthread_setspecific(key_rcon_fd, data);
		if(r)
		{
			fprintf(stderr, _("Cannot set thread specific data: %d\n"), r);
			free(data);
			data = NULL;
			goto cleanup;
		}
		data->fd = -1;
		data->connarg_hash_1 = 0;
		data->connarg_hash_2 = 0;
	}
	DEBUGF("rcon_host.c#rcon_host_get_current_thread_socket: Hash: %lu%lu -> %lu%lu.\n",
			data->connarg_hash_1,
			data->connarg_hash_2,
			connarg_existing_hash_1,
			connarg_existing_hash_2);

	if(data->connarg_hash_1 != connarg_existing_hash_1 || data->connarg_hash_2 != connarg_existing_hash_2)
	{
		DEBUGF("rcon_host.c#rcon_host_get_current_thread_socket: Hash mismatch (%lu%lu -> %lu%lu). Recreating.\n",
				data->connarg_hash_1,
				data->connarg_hash_2,
				connarg_existing_hash_1,
				connarg_existing_hash_2);
		if(data->fd != -1)
		{
			DEBUGF("rcon_host.c#rcon_host_get_current_thread_socket: Disconnecting existing socket %d.\n", data->fd);
			close(data->fd);
			data->fd = -1;
			data->connarg_hash_1 = 0;
			data->connarg_hash_2 = 0;
		}
		if(connarg != NULL)
		{
			struct rc_packet pkgt = {0, 0, 0, { 0x00 }};
			r = rcon_build_packet(&pkgt, RCON_PID, RCON_AUTHENTICATE, connarg->password);
			if(r) goto cleanup;
			int fd = -1;
			r = net_connect(connarg->host, connarg->port, &fd);
			if(r)
			{
				fprintf(stderr, _("Cannot connect to %s:%s: %d\n"), connarg->host, connarg->port, r);
				goto cleanup;
			}
			r = rcon_send_packet(fd, &pkgt);
			if(r)
			{
				close(fd);
				goto cleanup;
			}
			r = rcon_recv_packet(&pkgt, fd);
			if(r)
			{
				close(fd);
				goto cleanup;
			}
			if(pkgt.id == -1)
			{
				fprintf(stderr, _("Incorrect rcon password.\n"));
				close(fd);
				r = 77;
				goto cleanup;
			}
			data->fd = fd;
			data->connarg_hash_1 = connarg_existing_hash_1;
			data->connarg_hash_2 = connarg_existing_hash_2;
		}
		else
		{
			r = EPG_RCON_DISABLED;
		}
		DEBUGF("rcon_host.c#rcon_host_get_current_thread_socket: Socket updated: %d.\n", data->fd);
	}
	else if(data->fd == -1) /* Disabled */
	{
		r = EPG_RCON_DISABLED;
	}
	*out = data->fd;
	goto cleanup;
cleanup:
	return r;
}

int rcon_host_send(const int pkt_id, const char *command)
{
	int r = 0;
	int fd = 0;
	r = rcon_host_get_current_thread_socket(&fd);
	if(r) goto cleanup;
	struct rc_packet pkgt = {0, 0, 0, { 0x00 }};
	r = rcon_build_packet(&pkgt, pkt_id, RCON_EXEC_COMMAND, (char *)command);
	if(r) goto cleanup;
	r = rcon_send_packet(fd, &pkgt);
	if(r)
	{
		close(fd);
		rcon_host_clear_current_thread_socket();
		goto cleanup;
	}
	goto cleanup;
cleanup:
	return r;
}

int rcon_host_recv(int *pkt_id, char *out)
{
	int r = 0;
	int fd = 0;
	r = rcon_host_get_current_thread_socket(&fd);
	if(r) goto cleanup;
	struct rc_packet pkgt = {0, 0, 0, { 0x00 }};
	r = rcon_recv_packet(&pkgt, fd);
	if(r)
	{
		close(fd);
		rcon_host_clear_current_thread_socket();
		goto cleanup;
	}
	// TODO: Size issue? Memory issue?
	*pkt_id = pkgt.id;
	strcpy(out, pkgt.data);
	goto cleanup;
cleanup:
	return r;
}

int rcon_host_init()
{
	int r = 0;
	r = pthread_key_create(&key_rcon_fd, &destructor);
	if(r) goto cleanup;
	pthread_key_init = true;
cleanup:
	if(r) rcon_host_free();
	return r;
}

void rcon_host_free()
{
	if(pthread_key_init)
	{
		pthread_key_delete(key_rcon_fd);
		pthread_key_init = false;
	}
}

struct rcon_host_connarg *rcon_host_getconnarg()
{
	return connarg;
}

void rcon_host_setconnarg(struct rcon_host_connarg *arg)
{
	connarg = arg;
	connarg_hash_update(arg);
}

void rcon_host_connarg_free(struct rcon_host_connarg *arg)
{
	if(arg->host != NULL)
	{
		free(arg->host);
		arg->host = NULL;
	}
	if(arg->port != NULL)
	{
		free(arg->port);
		arg->port = NULL;
	}
	if(arg->password != NULL)
	{
		free(arg->password);
		arg->password = NULL;
	}
	free(arg);
}
