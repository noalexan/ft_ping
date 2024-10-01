#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAXIPLEN 60
#define MAXICMPLEN 76
#define PING_MAX_DATALEN (65535 - MAXIPLEN - MAXICMPLEN)

struct s_options {
	bool verbose;
	unsigned int count;
	unsigned int ttl;
	unsigned int timeout;
	unsigned int size;
	unsigned int interval;
};

struct ping_s {
	uint8_t *buffer;
	struct addrinfo *host;
	size_t packet_size;
	size_t sent_packet;
	size_t received_packet;
	uint16_t sequence;
};

extern struct s_options g_options;
extern bool stop;

extern int socket_fd;

void ft_ping(const char *host);
