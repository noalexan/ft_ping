#pragma once

#include <stdbool.h>

struct s_options
{
	bool   verbose;
	bool   debug;
	size_t size;
	size_t count;
};

extern struct s_options g_options;
extern bool running;

extern int socket_fd;

struct s_host
{
	char *host;
	struct s_host *next;
};

void ft_ping(struct s_host *host);
