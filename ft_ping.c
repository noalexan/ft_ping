#include "ft_ping.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static uint16_t compute_checksum(uint16_t *buffer, size_t len)
{
	int sum = 0;
	uint16_t *data = buffer;
	uint16_t *end = buffer + (len >> 1);

	while (data < end) sum += *data++;
	if (len & 1) sum += *(uint8_t *)data;
	while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);

	return ~((uint16_t)sum);
}

static struct addrinfo *dns_resolve(const char *hostname)
{
	struct addrinfo hints, *host;
	bzero(&hints, sizeof hints);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;

	int status = getaddrinfo(hostname, NULL, &hints, &host);

	if (status != 0) {
		fprintf(stderr, "getaddrinfo: %s (%i)\n", gai_strerror(status), status);
		exit(EXIT_FAILURE);
	}

	return host;
}

static void *create_buffer(size_t packet_size)
{
	void *buffer = malloc(packet_size);
	struct icmphdr *icmp = (struct icmphdr *)buffer;

	if (buffer == NULL) {
		perror("ft_ping: malloc");
		exit(EXIT_FAILURE);
	}

	bzero(icmp, sizeof(struct icmphdr));

	icmp->type = ICMP_ECHO;
	icmp->un.echo.id = htons(getpid());
	icmp->code = 0;

	for (size_t i = 8; i < packet_size; i++) ((unsigned char *)buffer)[i] = rand();

	return buffer;
}

static void send_ping(struct addrinfo *host, void *buffer, size_t packet_size)
{
	struct icmphdr *icmp = (struct icmphdr *)buffer;
	static uint16_t sequence = 0;

	icmp->un.echo.sequence = htons(sequence++);
	icmp->checksum = 0;
	icmp->checksum = compute_checksum((uint16_t *)buffer, packet_size);

	if (sendto(socket_fd, buffer, packet_size, 0, host->ai_addr, host->ai_addrlen) < 0) {
		perror("ft_ping: sending packet");
		exit(EXIT_FAILURE);
	}
}

void ft_ping(const char *hostname)
{
	struct addrinfo *host = dns_resolve(hostname);

	size_t packet_size = g_options.size + sizeof(struct icmphdr);

	void *send_buffer = create_buffer(packet_size);
	void *recv_buffer = malloc(0x10000);

	struct iphdr *recv_ip = (struct iphdr *)recv_buffer;
	struct icmphdr *send_icmp = (struct icmphdr *)send_buffer, *recv_icmp;

	size_t sent_packet = 0, received_packet = 0;
	struct timeval now, interval = {.tv_sec = 1, .tv_usec = 0}, response_timeout, last;
	double time = 0, min = INT_MAX, max = INT_MIN, total = 0;

	fd_set fdset;

	if (send_buffer == NULL) {
		perror("ft_ping: malloc");
		exit(EXIT_FAILURE);
	}

	else if (recv_buffer == NULL) {
		free(send_buffer);
		perror("ft_ping: malloc");
		exit(EXIT_FAILURE);
	}

	printf("PING %s (%s): %u data bytes", hostname, inet_ntoa((struct in_addr)((struct sockaddr_in *)host->ai_addr)->sin_addr), g_options.size);
	if (g_options.verbose)
		printf(", id 0x%x = %i", ntohs(send_icmp->un.echo.id), ntohs(send_icmp->un.echo.id));
	printf("\n");

	gettimeofday(&last, NULL);
	send_ping(host, send_buffer, packet_size);
	sent_packet++;

	while (!stop) {
		FD_ZERO(&fdset);
		FD_SET(socket_fd, &fdset);
		gettimeofday(&now, NULL);

		response_timeout = interval;

		int n = select(socket_fd + 1, &fdset, NULL, NULL, &response_timeout);

		if (n < 0) {
			if (errno != EINTR) {
				perror("ft_ping: select");
				exit(EXIT_FAILURE);
			}
			continue;
		}

		else if (n == 1) {
			struct sockaddr client;
			socklen_t client_len;
			ssize_t len = recvfrom(socket_fd, recv_buffer, 0x10000, 0, &client, &client_len);

			/* Reverse DNS */

			char *ipstr = inet_ntoa((struct in_addr)((struct sockaddr_in *)&client)->sin_addr),
				 hoststr[NI_MAXHOST], name[NI_MAXHOST + INET_ADDRSTRLEN + 2];

			int res = getnameinfo(&client, sizeof client, hoststr, sizeof hoststr, NULL, 0,
#ifdef NI_IDN
								  NI_IDN | NI_NAMEREQD
#else
								  NI_NAMEREQD
#endif
			);

			if (res)
				strncpy(name, ipstr, INET_ADDRSTRLEN);
			else
				sprintf(name, "%s (%s)", hoststr, ipstr);

			/* Did recvfrom return an error? */

			if (len < 0) {
				fprintf(stderr, "%zd bytes from %s: %s\n", len, name, strerror(errno));
				continue;
			}

			/* Header Setup */

			recv_ip = (struct iphdr *)recv_buffer;
			size_t hlen = recv_ip->ihl << 2;

			recv_icmp = (struct icmphdr *)(recv_buffer + hlen);

			/* ICMP Type check */

			switch (recv_icmp->type) {
			case ICMP_ECHOREPLY:
				break;

			case ICMP_ECHO:
				continue;

			case ICMP_TIME_EXCEEDED:
				switch (recv_icmp->code) {
				case ICMP_EXC_TTL:
					fprintf(stderr, "%zu bytes from %s: Time to live exceeded\n", len - hlen, name);
					continue;

				default:
					fprintf(stderr, "unknown code (%d) for ICMP_TIME_EXCEEDED\n", recv_icmp->code);
					continue;
				}
				continue;

			default:
				fprintf(stderr, "unknown type (%d)\n", recv_icmp->type);
				continue;
			}

			/* Packet Integrity Check */

			if (compute_checksum((uint16_t *)recv_buffer, len) != 0)
				fprintf(stderr, "checksum mismatch from %s\n", name);

			/* Resume Packet */

			received_packet++;

			gettimeofday(&last, NULL);

			time = (last.tv_sec - now.tv_sec) * 1000.0f + (last.tv_usec - now.tv_usec) / 1000.0f;

			total += time;

			max = fmax(max, time);
			min = fmin(min, time);

			printf("%zu bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n", len - hlen, name, ntohs(recv_icmp->un.echo.sequence), recv_ip->ttl, time);
		}

		else {
			if (!g_options.count || sent_packet < g_options.count) {
				send_ping(host, send_buffer, packet_size);
				sent_packet++;
			}

			else
				break;

			gettimeofday(&last, NULL);
		}
	}

	fflush(stdout);
	printf("--- %s ping statistics ---\n", hostname);
	printf("%zu packets transmitted, %zu packets received", sent_packet, received_packet);
	// if (ping->ping_num_rept)
	// 	printf(", +%zu duplicates", ping->ping_num_rept);

	if (sent_packet) {
		if (received_packet > sent_packet)
			printf(", -- somebody is printing forged packets!");
		else
			printf(", %d%% packet loss", (int)(((sent_packet - received_packet) * 100) / sent_packet));
	}

	printf("\n");

	free(send_buffer);
	free(recv_buffer);
	freeaddrinfo(host);
}
