#pragma once

#include <stdbool.h>

extern struct s_options
{
	bool verbose;
} g_options;

extern bool running;

struct s_host
{
	char *host;
	struct s_host *next;
};

void ft_ping(struct s_host *host);
