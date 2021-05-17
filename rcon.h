/*
 * Adopted from mcrcon, Copyright (c) 2012-2020, Tiiffi <tiiffi at gmail>.
 * https://github.com/Tiiffi/mcrcon/tree/b02201d689b3032bc681b28f175fd3d83d167293
 */

#ifndef _RCON_H
#define _RCON_H

#include "plugin/common.h"

#define RCON_EXEC_COMMAND       2
#define RCON_AUTHENTICATE       3
#define RCON_RESPONSEVALUE      0
#define RCON_AUTH_RESPONSE      2
#define RCON_PID                0xBADC0DE

struct rc_packet {
    int size;
    int id;
    int cmd;
    char data[RCON_DATA_BUFFSIZE];
    // ignoring string2 for now
};

int rcon_send_packet(int sd, struct rc_packet *packet);
int rcon_build_packet(struct rc_packet *out, int id, int cmd, char *s1);
int rcon_recv_packet(struct rc_packet *out, int sd);

#endif // _RCON_H
