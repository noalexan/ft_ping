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
#include <errno.h>
#include <math.h>
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

struct addrinfo *dns_resolve(const char *hostname)
{
	struct addrinfo hints, *host;
	bzero(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	int status = getaddrinfo(hostname, NULL, &hints, &host);

	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}

	return host;
}

static void *create_buffer(size_t packet_size)
{
	void *buffer = malloc(packet_size);
	struct icmphdr *icmp = (struct icmphdr *)buffer;

	if (buffer == NULL)
	{
		perror("ft_ping: malloc");
		exit(EXIT_FAILURE);
	}

	bzero(icmp, sizeof(struct icmphdr));

	icmp->type = ICMP_ECHO;
	icmp->un.echo.id = htons(getpid());
	icmp->code = 0;

	for (size_t i = 8; i < packet_size; i++)
		((unsigned char *)buffer)[i] = rand();

	return buffer;
}

static void send_ping(struct addrinfo *host, void *buffer, size_t packet_size)
{
	struct icmphdr *icmp = (struct icmphdr *)buffer;
	static uint16_t sequence = 0;

	icmp->un.echo.sequence = htons(sequence++);
	icmp->checksum = compute_checksum((uint16_t *)buffer, packet_size);

	if (sendto(socket_fd, buffer, packet_size, 0, host->ai_addr, host->ai_addrlen) < 0)
	{
		perror("ft_ping: sendto");
		exit(EXIT_FAILURE);
	}
}

static ssize_t receive_ping(struct addrinfo *host, void *const buffer)
{
	ssize_t len;

	do
	{
		if ((len = recvfrom(socket_fd, buffer, 0x10000, 0, host->ai_addr, &host->ai_addrlen)) < 0)
		{
			fprintf(stderr, "%zd bytes from %s: %s\n",
					len,
					inet_ntoa((struct in_addr)((struct sockaddr_in *)host->ai_addr)->sin_addr),
					strerror(errno));
			break;
		}

		printf("received packet with length %lu.\n", len);

		struct iphdr *return_ip = (struct iphdr *)buffer;
		size_t hlen = return_ip->ihl << 2;

		if (return_ip->protocol != 0x01)
		{
			printf("not an icmp packet.\n");
			continue;
		}

		struct icmphdr *return_icmp = (struct icmphdr *)(buffer + hlen);

		switch (return_icmp->type)
		{
		case ICMP_ECHOREPLY:
			printf("type: ICMP_ECHOREPLY\n");
			return len;

		case ICMP_ECHO:
			printf("type: ICMP_ECHO\n");
			break;

		case ICMP_TIME_EXCEEDED:
			printf("type: ICMP_TIME_EXCEEDED\n");
			switch (return_icmp->code)
			{
			case ICMP_EXC_TTL:
				fprintf(stderr, "%zu bytes from %s: Time to live exceeded\n",
						len - hlen,
						inet_ntoa((struct in_addr)((struct sockaddr_in *)host->ai_addr)->sin_addr));
				break;

			default:
				fprintf(stderr, "unknown code (%d) for ICMP_TIME_EXCEEDED\n", return_icmp->code);
				break;
			}
			break;

		default:
			fprintf(stderr, "unknown type (%d)\n", return_icmp->type);
			continue;
		}
	} while (true);

	return -1;
}

void ft_ping(const char *hostname)
{
	struct addrinfo *host = dns_resolve(hostname);

	size_t packet_size = g_options.size + sizeof(struct icmphdr);

	void *buffer = create_buffer(packet_size);
	void *return_buffer = malloc(0x10000);

	struct iphdr *return_ip = (struct iphdr *)return_buffer;
	struct icmphdr *icmp = (struct icmphdr *)buffer, *return_icmp = (struct icmphdr *)return_buffer;

	size_t count = 0, packet_sent = 0, packet_received = 0;
	struct timeval start, end;
	double time = 0, min = 0, max = 0, total = 0;

	if (buffer == NULL)
	{
		perror("ft_ping: malloc:");
		exit(EXIT_FAILURE);
	}

	printf("PING %s (%s): %zu data bytes", hostname, inet_ntoa((struct in_addr)((struct sockaddr_in *)host->ai_addr)->sin_addr), g_options.size);
	if (g_options.verbose)
		printf(", id 0x%x = %i", ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.id));
	printf("\n");

	while (running && count++ < g_options.count)
	{

		/* Sending */

		send_ping(host, buffer, packet_size);
		packet_sent++;

		/* Receiving */

		gettimeofday(&start, NULL);
		ssize_t len = receive_ping(host, return_buffer);
		gettimeofday(&end, NULL);

		time = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;

		return_ip = (struct iphdr *)return_buffer;
		size_t hlen = return_ip->ihl << 2;
		return_icmp = (struct icmphdr *)(return_buffer + hlen);

		total += time;

		if (count == 1)
		{
			min = time;
			max = time;
		}

		max = fmax(max, time);
		min = fmin(min, time);

		if (icmp->un.echo.sequence == return_icmp->un.echo.sequence)
			packet_received++;

		if (compute_checksum((uint16_t *)return_buffer, len) != 0)
		{
			fprintf(stderr, "checksum mismatch from %s\n",
					inet_ntoa((struct in_addr)((struct sockaddr_in *)host->ai_addr)->sin_addr));
		}

		printf("%zu bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
			   len - hlen,
			   inet_ntoa((struct in_addr)((struct sockaddr_in *)host->ai_addr)->sin_addr),
			   ntohs(return_icmp->un.echo.sequence),
			   return_ip->ttl,
			   time);

		/* Waiting */

		if (running && count < g_options.count)
			usleep(1000000);
	}

	printf(
		"--- %s ping statistics ---\n"
		"%zu packets transmitted, %zu packets received, %zu%% packet loss\n"
		"round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
		hostname,
		packet_sent,
		packet_received,
		(packet_sent - packet_received) * 100 / packet_sent,
		min,
		total / packet_received,
		max,
		0.0);

	free(buffer);
	free(return_buffer);
	freeaddrinfo(host);
}
