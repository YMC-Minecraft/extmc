#ifndef _RCON_HOST_H
#define _RCON_HOST_H

struct rcon_host_connarg {
	char *host;
	char *port;
	char *password;
};

int rcon_host_init();
void rcon_host_free();
void rcon_host_connarg_free(struct rcon_host_connarg *arg);

int rcon_host_send(const int id, const char *command);
int rcon_host_recv(int *pkgt_id, char *out);

void rcon_host_setconnarg(struct rcon_host_connarg *arg);
struct rcon_host_connarg *rcon_host_getconnarg();

#endif // _RCON_HOST_H
