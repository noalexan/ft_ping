#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "ft_ping.h"

struct s_options g_options;
struct s_host *hosts = NULL;
char *program_name;
bool running = true;
int socket_fd;

void help()
{
	printf(
			"Usage: %s [OPTION...] HOST ...\n"
			"Send ICMP ECHO_REQUEST packets to network hosts.\n"
			"\n"
			" Options valid for all request types:\n"
			"\n"
			"  -v, --verbose              verbose output\n"
			"\n"
			"  -?, --help                 give this help list\n"
			"      --usage                give a short usage message\n"
			"  -V, --version              print program version\n"
			"\n"
			"Mandatory or optional arguments to long options are also mandatory or optional\n"
			"for any corresponding short options.\n"
			"\n"
			"Report bugs to <noalexan@student.42nice.fr>.\n",
			program_name);
	exit(EXIT_SUCCESS);
}

void usage()
{
	printf("Usage: %s [-v?V] [--help] [--usage] [--version] HOST ...\n", program_name);
	exit(EXIT_SUCCESS);
}

void version()
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

	if (close(socket_fd) < 0)
	{
		perror("ft_ping: close");
	}
}

void stop()
{
	running = false;
}

int main(int argc, char **argv)
{
	program_name = *argv;

	if (program_name == NULL)
	{
		fputs("A NULL argv[0] was passed through an exec system call.\n",
					stderr);
		abort();
	}

	g_options = (struct s_options){
			.verbose = false,
			.debug   = false,
			.size    = 56,
			.count   = -1,
	};

	bool argument_parsing = true;
	for (int i = 1; i < argc; i++)
	{
		if (argument_parsing && argv[i][0] == '-' && argv[i][1])
		{
			if (argv[i][1] != '-')
			{
				for (int j = 1; argv[i][j]; j++)
				{
					if (argv[i][j] == 'v')
					{
						g_options.verbose = true;
					}

					else if (argv[i][j] == '?')
					{
						help();
					}

					else if (argv[i][j] == 'V')
					{
						version();
					}

					else if (argv[i][j] == 'd')
					{
						g_options.debug = true;
					}

					else if (argv[i][j] == 's')
					{
						char *endptr, *strtptr = argv[i][j + 1] ? &argv[i][++j] : argv[++i];
						if (strtptr == NULL)
						{
							fprintf(stderr,
											"%s: option requires an argument -- 's'\n"
											"Try 'ping --help' or 'ping --usage' for more information.\n",
											program_name);
							exit(EXIT_FAILURE);
						}
						g_options.size = strtoul(strtptr, &endptr, 10);
						if (*endptr)
						{
							fprintf(stderr, "%s: invalid value (`%s' near `%s')\n",
											program_name, strtptr, endptr);
							exit(EXIT_FAILURE);
						}
						break;
					}

					else if (argv[i][j] == 'c')
					{
						char *endptr, *strtptr = argv[i][j + 1] ? &argv[i][++j] : argv[++i];
						if (strtptr == NULL)
						{
							fprintf(stderr,
											"%s: option requires an argument -- 'c'\n"
											"Try 'ping --help' or 'ping --usage' for more information.\n",
											program_name);
							exit(EXIT_FAILURE);
						}
						g_options.count = strtoul(strtptr, &endptr, 10);
						if (*endptr)
						{
							fprintf(stderr, "%s: invalid value (`%s' near `%s')\n",
											program_name, strtptr, endptr);
							exit(EXIT_FAILURE);
						}
						break;
					}

					else
					{
						fprintf(stderr,
										"%s: invalid option -- '%c'\n"
										"Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n",
										program_name, argv[i][j]);
						exit(EXIT_FAILURE);
					}
				}
			}

			else
			{
				if (strcmp(argv[i], "--") == 0)
				{
					argument_parsing = false;
				}

				else if (strcmp(argv[i], "--verbose") == 0)
				{
					g_options.verbose = true;
				}

				else if (strcmp(argv[i], "--help") == 0)
				{
					help();
				}

				else if (strcmp(argv[i], "--usage") == 0)
				{
					usage();
				}

				else if (strcmp(argv[i], "--version") == 0)
				{
					version();
				}

				else if (strcmp(argv[i], "--debug") == 0)
				{
					g_options.debug = true;
				}

				else if (strncmp(argv[i], "--size", 6) == 0 && (argv[i][6] == 0 || argv[i][6] == '='))
				{
					char *endptr;
					g_options.size = strtoul(argv[i][6] == 0 ? argv[++i] : argv[i] + 7, &endptr, 10);
					if (*endptr)
					{
						fprintf(stderr, "%s: invalid value (`%s' near `%s')\n",
										program_name, argv[i][6] == 0 ? argv[++i] : argv[i] + 7, endptr);
						exit(EXIT_FAILURE);
					}
				}

				else if (strncmp(argv[i], "--count", 7) == 0 && (argv[i][7] == 0 || argv[i][7] == '='))
				{
					char *endptr;
					g_options.count = strtoul(argv[i][7] == 0 ? argv[++i] : argv[i] + 8, &endptr, 10);
					if (*endptr)
					{
						fprintf(stderr, "%s: invalid value (`%s' near `%s')\n",
										program_name, argv[i][7] == 0 ? argv[++i] : argv[i] + 8, endptr);
						exit(EXIT_FAILURE);
					}
				}

				else
				{
					fprintf(stderr,
									"%s: unrecognized option '%s'\n"
									"Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n",
									program_name, argv[i]);
					exit(EXIT_FAILURE);
				}
			}
		}

		else
		{
			struct s_host *new = add_new_host();
			new->host = argv[i];
		}
	}

	if (hosts == NULL)
	{
		fprintf(stderr,
						"%s: missing host operand\n"
						"Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n",
						program_name);
		exit(EXIT_FAILURE);
	}

	// printf(
	// 		"size:  %lu\n"
	// 		"count: %lu\n",
	// 		g_options.size,
	// 		g_options.count);

	// exit(EXIT_SUCCESS);

	atexit(cleanup);
	signal(SIGINT, stop);

	struct protoent *proto;

	proto = getprotobyname("icmp");
	if (proto == NULL)
	{
		fprintf(stderr, "%s: unknown protocol icmp.\n", program_name);
		exit(EXIT_FAILURE);
	}

	socket_fd = socket(AF_INET, SOCK_DGRAM, proto->p_proto);

	if (socket_fd < 0)
	{
		perror("ft_ping: socket");
		exit(EXIT_FAILURE);
	}

	struct timeval timeout;
	timeout.tv_sec  = 10;
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
