#pragma once

#include <stdbool.h>

#define MAXIPLEN 60
#define MAXICMPLEN 76
#define PING_MAX_DATALEN (65535 - MAXIPLEN - MAXICMPLEN)

struct s_options {
	bool verbose;
	unsigned int count;
	unsigned int ttl;
	unsigned int timeout;
	unsigned int size;
	unsigned int interval;
};

extern struct s_options g_options;
extern bool stop;

extern int socket_fd;

void ft_ping(const char *host);
