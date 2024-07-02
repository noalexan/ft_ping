#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "ft_ping.h"

struct s_options g_options;
struct s_host *hosts = NULL;
char *program_name;
bool running = true;
int socket_fd;

static void help()
{
	printf(
			"Usage: ft_ping [OPTION...] HOST ...\n"
			"Send ICMP ECHO_REQUEST packets to network hosts.\n"
			"\n"
			" Options valid for all request types:\n"
			"\n"
			"  -c, --count=NUMBER         stop after sending NUMBER packets\n"
			"  -d, --debug                set the SO_DEBUG option\n"
			"      --ttl=N                specify N as time-to-live\n"
			"  -v, --verbose              verbose output\n"
			"  -w, --timeout=N            stop after N seconds\n"
			"\n"
			" Options valid for --echo requests:\n"
			"\n"
			"  -s, --size=NUMBER          send NUMBER data octets\n"
			"\n"
			"  -?, --help                 give this help list\n"
			"      --usage                give a short usage message\n"
			"  -V, --version              print program version\n"
			"\n"
			"Mandatory or optional arguments to long options are also mandatory or optional\n"
			"for any corresponding short options.\n"
			"\n"
			"Report bugs to <noalexan@student.42nice.fr>.\n");
	exit(EXIT_SUCCESS);
}

static void usage()
{
	printf(
			"Usage: ft_ping [-dv?V] [-c NUMBER] [-w N] [-s NUMBER]\n"
			"            [--count=NUMBER] [--debug] [--ttl=N]\n"
			"            [--verbose] [--timeout=N] [--size=NUMBER]\n"
			"            [--help] [--usage] [--version]\n"
			"            HOST ...\n");
	exit(EXIT_SUCCESS);
}

static void version()
{
	printf(
			"ft_ping based on ping (GNU inetutils) 2.0\n"
			"\n"
			"Written by Noah Alexandre.\n");
	exit(EXIT_SUCCESS);
}

static struct s_host *add_new_host()
{
	struct s_host *new = calloc(1, sizeof(struct s_host));
	struct s_host *iter = hosts;

	if (new == NULL)
	{
		perror("ft_ping: calloc");
		exit(EXIT_FAILURE);
	}

	if (hosts == NULL)
	{
		hosts = new;
	}

	else
	{
		while (iter->next)
		{
			iter = iter->next;
		}

		iter->next = new;
	}

	return new;
}

void cleanup()
{
	if (hosts)
	{
		while (hosts->next != NULL)
		{
			struct s_host *iter = hosts;

			while (iter->next && iter->next->next)
				iter = iter->next;

			free(iter->next);
			iter->next = NULL;
		}

		free(hosts);
		hosts = NULL;
	}

	if (close(socket_fd) < 0)
	{
		perror("ft_ping: close");
	}
}

static void stop()
{
	running = false;
}

static size_t take_arg()
{
	char *endptr;
	size_t arg = strtoul(optarg, &endptr, 10);

	if (*endptr)
	{
		fprintf(stderr, "%s: invalid value (`%s' near `%s')\n",
						program_name, optarg, endptr);
		exit(EXIT_FAILURE);
	}

	return arg;
}

int main(int argc, char **argv)
{
	signal(SIGINT, stop);

	if ((program_name = *argv) == NULL)
	{
		fputs("A NULL argv[0] was passed through an exec system call.\n",
					stderr);
		abort();
	}

	g_options = (struct s_options){
			.verbose = false,
			.debug = false,
			.size = 56,
			.count = -1,
			.timeout = -1,
	};

	struct protoent *proto;

	proto = getprotobyname("icmp");
	if (proto == NULL)
	{
		fprintf(stderr, "ft_ping: unknown protocol icmp.\n");
		exit(EXIT_FAILURE);
	}

	socket_fd = socket(AF_INET, SOCK_DGRAM, proto->p_proto);

	if (socket_fd < 0)
	{
		perror("ft_ping: socket");
		exit(EXIT_FAILURE);
	}

	atexit(cleanup);

	while (true)
	{
		static struct option long_options[] = {
				{"count", required_argument, 0, 'c'},
				{"debug", no_argument, 0, 'd'},
				{"ttl", required_argument, 0, 't'},
				{"verbose", no_argument, 0, 'v'},
				{"timeout", required_argument, 0, 'w'},
				{"size", required_argument, 0, 's'},
				{"help", no_argument, 0, '?'},
				{"usage", no_argument, 0, 'u'},
				{"version", no_argument, 0, 'V'},
				{0, 0, 0, 0},
		};

		int option_index;
		int c = getopt_long(argc, argv, "c:dvw:s:?V", long_options, &option_index);

		if (c == -1)
			break;

		switch (c)
		{
		case 'c':
			g_options.count = take_arg();
			break;

		case 'd':
			g_options.debug = true;
			break;

		case 'v':
			g_options.verbose = true;
			break;

		case 'w':
			g_options.timeout = take_arg();
			break;

		case 's':
			g_options.size = take_arg();
			break;

		case 'u':
			usage();

		case 'V':
			version();

		default:
			if (optopt == 0)
				help();

			fprintf(stderr, "Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n");
			exit(EXIT_FAILURE);
		}
	}

	while (optind < argc)
	{
		struct s_host *new = add_new_host();
		new->host = argv[optind++];
	}

	if (hosts == NULL)
	{
		fprintf(stderr,
						"ft_ping: missing host operand\n"
						"Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n");
		exit(EXIT_FAILURE);
	}

	if (g_options.count == 0)
		g_options.count = -1;

	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	int on = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0 || setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0)
	{
		perror("ft_ping: setsockopt");
	}

	if (g_options.debug && setsockopt(socket_fd, SOL_SOCKET, SO_DEBUG, &on, sizeof on) < 0)
	{
		perror("ft_ping: setsockopt");
	}

	struct s_host *iter = hosts;

	while (iter)
	{
		ft_ping(iter);
		iter = iter->next;
	}

	return 0;
}
