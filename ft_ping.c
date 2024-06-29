#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include "ft_ping.h"

static uint16_t compute_checksum(uint16_t *buffer, size_t len)
{
	int sum = 0;
	uint16_t *data = buffer;
	uint16_t *end = buffer + (len >> 1);

	while (data < end)
		sum += *data++;
	if (len & 1)
		sum += *(uint8_t *)data;
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~((uint16_t)sum);
}

void ft_ping(struct s_host *host)
{
	struct addrinfo hints, *res;
	bzero(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	int status = getaddrinfo(host->host, NULL, &hints, &res);

	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	printf("PING %s (%s): %lu data bytes\n", host->host, inet_ntoa((struct in_addr)((struct sockaddr_in *)res->ai_addr)->sin_addr), g_options.size);

	size_t packet_size = g_options.size + sizeof(struct icmphdr);
	uint8_t *buffer = malloc(packet_size), return_buffer[0x10000];
	struct icmphdr *icmp = (struct icmphdr *)buffer, *return_icmp;

	for (size_t i = 8; i < packet_size; i++)
	{
		buffer[i] = rand() % 0x100;
	}

	bzero(icmp, sizeof(struct icmphdr));

	icmp->type = ICMP_ECHO;
	icmp->code = 0;

	uint16_t sequence = 0;
	size_t count = 0, len, packet_sent = 0, packet_received = 0;

	struct timeval start, end;
	float time, min, max, total = 0;

	while (count++ < g_options.count && running)
	{
		icmp->un.echo.sequence = sequence++;
		icmp->un.echo.id = rand() % 0x10000;

		icmp->checksum = compute_checksum((uint16_t *)buffer, packet_size);

		gettimeofday(&start, NULL);

		if (sendto(socket_fd, buffer, packet_size, 0, res->ai_addr, res->ai_addrlen) < 0)
		{
			perror("ft_ping: sendto");
			exit(EXIT_FAILURE);
		}

		packet_sent++;

		if ((len = recvfrom(socket_fd, return_buffer, 0x10000, 0, res->ai_addr, &res->ai_addrlen)) < 0)
		{
			perror("ft_ping: recvfrom");
			exit(EXIT_FAILURE);
		}

		return_icmp = (struct icmphdr *)return_buffer;

		if (return_icmp->type != ICMP_ECHOREPLY || return_icmp->code != 0)
			continue;

		gettimeofday(&end, NULL);

		packet_received++;

		time = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
		total += time;

		printf("%li bytes from %s: icmp_seq=%i ttl=0 time=%.3f ms\n",
					len,
					inet_ntoa((struct in_addr)((struct sockaddr_in *)res->ai_addr)->sin_addr),
					return_icmp->un.echo.sequence,
					time);

		if (count < g_options.count && running)
			usleep(1000000);
	}

	printf(
			"--- %s ping statistics ---\n"
			"%lu packets transmitted, %lu packets received, %i%% packet loss\n"
			"round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			host->host,
			packet_sent,
			packet_received,
			100,
			0,
			total / packet_received,
			0,
			0);

	free(buffer);
	freeaddrinfo(res);
}
