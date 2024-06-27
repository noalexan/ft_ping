#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "ft_ping.h"

void ft_ping(struct s_host *host)
{
	struct addrinfo hints, *res;
	bzero(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;

	int status = getaddrinfo(host->host, NULL, &hints, &res);

	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(res);
}
