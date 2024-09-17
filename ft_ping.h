#pragma once

#include <stdbool.h>

#define MAXIPLEN 60
#define MAXICMPLEN 76
#define PING_MAX_DATALEN (65535 - MAXIPLEN - MAXICMPLEN)

struct s_options
{
	size_t count;
	int ttl;
	bool verbose;
	size_t timeout;
	size_t size;
};

extern struct s_options g_options;
extern bool stop;

extern int socket_fd;

void ft_ping(const char *host);
