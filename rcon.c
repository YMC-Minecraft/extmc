/*
 * Adopted from mcrcon, Copyright (c) 2012-2020, Tiiffi <tiiffi at gmail>.
 * https://github.com/Tiiffi/mcrcon/tree/b02201d689b3032bc681b28f175fd3d83d167293
 */

#include "rcon.h"
#include "common.h"

#include <sys/socket.h>
#include <sysexits.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int rcon_send_packet(int sd, struct rc_packet *packet)
{
        int len;
        int total = 0;  // bytes we've sent
        int bytesleft;  // bytes left to send
        int ret = -1;

        bytesleft = len = packet->size + sizeof(int);

        while (total < len)
	{
                ret = send(sd, (char *) packet + total, bytesleft, 0);
                if(ret == -1)
		{
			fprintf(stderr, _("send(): %s.\n"), strerror(errno));
			return EX_IOERR;
		}
                total += ret;
                bytesleft -= ret;
        }

	return EX_OK;
}

int rcon_build_packet(struct rc_packet *out, int id, int cmd, char *s1)
{
        // size + id + cmd + s1 + s2 NULL terminator
        int s1_len = strlen(s1);
        if (s1_len > RCON_DATA_BUFFSIZE)
	{
                fprintf(stderr, _("Warning: Command string too long (%d). Maximum allowed: %d.\n"), s1_len, RCON_DATA_BUFFSIZE);
                return EX_DATAERR;
        }

        out->size = sizeof(int) * 2 + s1_len + 2;
        out->id = id;
        out->cmd = cmd;
        strncpy(out->data, s1, RCON_DATA_BUFFSIZE);

	return EX_OK;
}

int rcon_recv_packet(struct rc_packet *out, int sd)
{
        int psize;

        int ret = recv(sd, (char *) &psize, sizeof(int), 0);

        if (ret == 0)
	{
                fprintf(stderr, _("Connection lost.\n"));
                return EX_IOERR;
        }
	if(ret == -1)
	{
		fprintf(stderr, _("recv(): %d\n"), errno);
		return EX_IOERR;
	}

        if (ret != sizeof(int))
	{
                fprintf(stderr, _("Error: recv() failed. Invalid packet size (%d).\n"), ret);
                return EX_IOERR;
        }

        if (psize < 10 || psize > RCON_DATA_BUFFSIZE)
	{
                fprintf(stderr, _("Warning: invalid packet size (%d). Must over 10 and less than %d.\n"), psize, RCON_DATA_BUFFSIZE);

                if(psize > RCON_DATA_BUFFSIZE  || psize < 0) psize = RCON_DATA_BUFFSIZE;
		// Former net_clean_incoming.
		char tmp[psize];
		ret = recv(sd, tmp, psize, 0);

	        if(ret == 0)
		{
			fprintf(stderr, _("Connection lost.\n"));
		}

                return EX_DATAERR;
        }

        out->size = psize;

        int received = 0;
        while (received < psize)
	{
                ret = recv(sd, (char *) out + sizeof(int) + received, psize - received, 0);
                if (ret == 0) // connection closed before completing receving
		{
                        fprintf(stderr, _("Connection lost.\n"));
                        return EX_IOERR;
                }

                received += ret;
        }

        return EX_OK;
}
