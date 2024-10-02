#include "ft_ping.h"

#include <arpa/inet.h>
#include <errno.h>
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

	if (status)
		return NULL;

	return host;
}

static void *create_buffer(size_t packet_size)
{
	uint8_t *buffer = malloc(packet_size);
	struct icmphdr *icmp = (struct icmphdr *)buffer;

	if (buffer == NULL) {
		perror("ft_ping: malloc");
		exit(EXIT_FAILURE);
	}

	bzero(icmp, sizeof(struct icmphdr));

	icmp->type = ICMP_ECHO;
	icmp->un.echo.id = htons(getpid());
	icmp->code = 0;

	for (size_t i = 8; i < packet_size; i++)
		buffer[i] = rand();

	return buffer;
}

static void send_ping(struct ping_s *ping)
{
	struct icmphdr *icmp = (struct icmphdr *)ping->buffer;

	icmp->un.echo.sequence = htons(ping->sequence++);
	icmp->checksum = 0;
	icmp->checksum = compute_checksum((uint16_t *)ping->buffer, ping->packet_size);

	if (sendto(socket_fd, ping->buffer, ping->packet_size, 0, ping->host->ai_addr, ping->host->ai_addrlen) < 0) {
		perror("ft_ping: sending packet");
		exit(EXIT_FAILURE);
	}
}

static void ft_packet_dump(const uint8_t *buffer)
{
	const struct iphdr *ip = (struct iphdr *)buffer;
	size_t hlen = ip->ihl << 2;
	const uint8_t *cp = (uint8_t *)ip + hlen;

	printf("IP Hdr Dump:\n");
	for (int i = 0; i < 10; i++)
		printf(" %04x", ((uint16_t *)ip)[i]);
	printf("\nVr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData\n");
	printf(" %1x  %1x  %02x", ip->version, ip->ihl, ip->tos);
	printf(" %04x %04x", (ip->tot_len > 0x2000) ? ntohs(ip->tot_len) : ip->tot_len, ntohs(ip->id));
	printf("   %1x %04x", (ntohs(ip->frag_off) & 0xe000) >> 13, ntohs(ip->frag_off) & 0x1fff);
	printf("  %02x  %02x %04x", ip->ttl, ip->protocol, ntohs(ip->check));
	printf(" %s ", inet_ntoa(*((struct in_addr *)&ip->saddr)));
	printf(" %s ", inet_ntoa(*((struct in_addr *)&ip->daddr)));
	while (hlen-- > sizeof(struct iphdr))
		printf("%02x", *cp++);
	printf("\n");

	int type = *cp;
	int code = *(cp + 1);

	printf("ICMP: type %u, code %u, size %lu", type, code, ntohs(ip->tot_len) - hlen);
	if (type == ICMP_ECHOREPLY || type == ICMP_ECHO)
		printf(", id 0x%04x, seq 0x%04x", *(cp + 4) * 256 + *(cp + 5), *(cp + 6) * 256 + *(cp + 7));
	printf("\n");
}

void ft_ping(const char *hostname)
{
	bool finishing = false;
	struct ping_s ping;

	void *recv_buffer = malloc(0x10000);
	struct iphdr *recv_ip = (struct iphdr *)recv_buffer;
	struct icmphdr *send_icmp, *recv_icmp;

	struct timeval now, interval = {.tv_sec = 1, .tv_usec = 0}, response_timeout, last;
	double time = 0, min = INT_MAX, max = INT_MIN, total = 0;

	fd_set fdset;

	bzero(&ping, sizeof(struct ping_s));
	ping.host = dns_resolve(hostname);
	ping.packet_size = g_options.size + sizeof(struct icmphdr);
	ping.buffer = create_buffer(ping.packet_size);

	send_icmp = (struct icmphdr *)ping.buffer;

	if (ping.host == NULL) {
		fprintf(stderr, "ft_ping: unknown host\n");
		if (ping.buffer)
			free(ping.buffer);
		if (recv_buffer)
			free(recv_buffer);
		exit(EXIT_FAILURE);
	}

	else if (ping.buffer == NULL) {
		perror("ft_ping: malloc");
		freeaddrinfo(ping.host);
		if (recv_buffer)
			free(recv_buffer);
		exit(EXIT_FAILURE);
	}

	else if (recv_buffer == NULL) {
		freeaddrinfo(ping.host);
		free(ping.buffer);
		perror("ft_ping: malloc");
		exit(EXIT_FAILURE);
	}

	printf("PING %s (%s): %u data bytes", hostname, inet_ntoa((struct in_addr)((struct sockaddr_in *)ping.host->ai_addr)->sin_addr), g_options.size);
	if (g_options.verbose)
		printf(", id 0x%x = %i", ntohs(send_icmp->un.echo.id), ntohs(send_icmp->un.echo.id));
	printf("\n");

	gettimeofday(&last, NULL);
	send_ping(&ping);
	ping.sent_packet++;

	while (!stop) {
		FD_ZERO(&fdset);
		FD_SET(socket_fd, &fdset);
		gettimeofday(&now, NULL);

		timersub(&now, &last, &response_timeout);

		if (response_timeout.tv_usec >= 100)
			fprintf(stderr, "- bytes from nowhere: ICMP echo timed out\n");

		timeradd(&response_timeout, &interval, &response_timeout);

		while (response_timeout.tv_usec < 0) {
			response_timeout.tv_usec += 1000000;
			response_timeout.tv_sec--;
		}

		while (response_timeout.tv_usec >= 1000000) {
			response_timeout.tv_usec -= 1000000;
			response_timeout.tv_sec++;
		}

		if (response_timeout.tv_sec < 0)
			response_timeout.tv_sec = response_timeout.tv_usec = 0;

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

			char ipstr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, (void *)(&((struct sockaddr_in *)&client)->sin_addr), ipstr, 16);
			char hoststr[NI_MAXHOST], name[NI_MAXHOST + INET_ADDRSTRLEN + 2];

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

			case ICMP_DEST_UNREACH:
				fprintf(stderr, "%zu bytes from %s: Destination Host Unreachable\n", len - hlen, name);
				if (g_options.verbose)
					ft_packet_dump(recv_buffer);
				continue;

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

			ping.received_packet++;

			gettimeofday(&last, NULL);

			time = (last.tv_sec - now.tv_sec) * 1000.0f + (last.tv_usec - now.tv_usec) / 1000.0f;

			total += time;

			max = fmax(max, time);
			min = fmin(min, time);

			printf("%zu bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n", len - hlen, name, ntohs(recv_icmp->un.echo.sequence), recv_ip->ttl, time);
		}

		else {
			if (!g_options.count || ping.sent_packet < g_options.count) {
				send_ping(&ping);
				ping.sent_packet++;
			}

			else if (finishing)
				break;

			else {
				interval.tv_sec = 0;
				finishing = true;
			}

			gettimeofday(&last, NULL);
		}
	}

	fflush(stdout);
	printf("--- %s ping statistics ---\n", hostname);
	printf("%zu packets transmitted, %zu packets received", ping.sent_packet, ping.received_packet);

	if (ping.sent_packet) {
		if (ping.received_packet > ping.sent_packet)
			printf(", -- somebody is printing forged packets!");
		else
			printf(", %d%% packet loss", (int)(((ping.sent_packet - ping.received_packet) * 100) / ping.sent_packet));
	}

	printf("\n");

	free(ping.buffer);
	free(recv_buffer);
	freeaddrinfo(ping.host);
}
