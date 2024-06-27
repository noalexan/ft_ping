#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "ft_ping.h"

void ft_ping(struct s_host *host)
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(host->host, NULL, &hints, &res);

	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	void *addr;
	char ipstr[INET_ADDRSTRLEN];

	struct sockaddr_in *ipv4 = (struct sockaddr_in *) res->ai_addr;
	addr = &ipv4->sin_addr;

	inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
	printf("The IP address for %s is: %s\n", host->host, ipstr);

	freeaddrinfo(res);
}
