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

void ft_ping(const char *host)
{
	struct addrinfo hints, *res;
	bzero(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	int status = getaddrinfo(host, NULL, &hints, &res);

	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	printf("PING %s (%s): %lu data bytes\n", host, inet_ntoa((struct in_addr)((struct sockaddr_in *)res->ai_addr)->sin_addr), g_options.size);

	size_t packet_size = g_options.size + sizeof(struct icmphdr);
	uint8_t *buffer = malloc(packet_size), return_buffer[0x10000];
	struct icmphdr *icmp = (struct icmphdr *)buffer, *return_icmp;

	for (size_t i = 8; i < packet_size; i++)
	{
		buffer[i] = rand();
	}

	bzero(icmp, sizeof(struct icmphdr));

	icmp->type = ICMP_ECHO;
	icmp->code = 0;

	uint16_t sequence = 0;
	size_t count = 0, packet_sent = 0, packet_received = 0;
	ssize_t len;

	struct timeval start, end;
	double time = 0, min = 0, max = 0, total = 0;

	while (running && count++ < g_options.count)
	{
		icmp->un.echo.sequence = htons(sequence++);
		icmp->un.echo.id = htons(rand());

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

		struct iphdr *return_ip = (struct iphdr *) return_buffer;
		size_t hlen = return_ip->ihl << 2;

		return_icmp = (struct icmphdr *)(return_buffer + hlen);

		if (return_icmp->type != ICMP_ECHOREPLY || return_icmp->code != 0)
			continue;

		gettimeofday(&end, NULL);

		packet_received++;

		time = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
		total += time;

		if (count == 1)
		{
			min = time;
			max = time;
		}

		else if (time > max)
			max = time;
		else if (time < min)
			min = time;

		printf("%lu bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
					len - hlen,
					inet_ntoa((struct in_addr)((struct sockaddr_in *)res->ai_addr)->sin_addr),
					ntohs(return_icmp->un.echo.sequence),
					return_ip->ttl,
					time);

		if (running && count < g_options.count)
			usleep(1000000);
	}

	printf(
			"--- %s ping statistics ---\n"
			"%lu packets transmitted, %lu packets received, %lu%% packet loss\n"
			"round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			host,
			packet_sent,
			packet_received,
			(packet_sent - packet_received) * 100 / packet_sent,
			min,
			total / packet_received,
			max,
			0.0);

	free(buffer);
	freeaddrinfo(res);
}
