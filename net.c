/*
 * Adopted from mcrcon, Copyright (c) 2012-2020, Tiiffi <tiiffi at gmail>.
 * https://github.com/Tiiffi/mcrcon/tree/b02201d689b3032bc681b28f175fd3d83d167293
 */

#include "net.h"
#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sysexits.h>
#include <unistd.h>

int net_connect(const char *host, const char *port, int *out)
{
	int sd;
	struct addrinfo hints;
	struct addrinfo *server_info, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	int ret = getaddrinfo(host, port, &hints, &server_info);
	if(ret)
	{
		fprintf(stderr, _("Cannot resolve host %s: %s.\n"), host, strerror(ret));
		return EX_IOERR;
	}
	for (p = server_info; p != NULL; p = p->ai_next)
	{
		sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sd == -1) continue;
		ret = connect(sd, p->ai_addr, p->ai_addrlen);
		if(ret == -1)
		{
			close(sd);
			continue;
		}
		break;
	}
	if(p == NULL)
	{
		fprintf(stderr, _("Cannot connect to %s:%s : %s.\n"), host, port, strerror(errno));
		freeaddrinfo(server_info);
		return EX_IOERR;
	}
	freeaddrinfo(server_info);
	*out = sd;
	return 0;
}
