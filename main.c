#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct s_options
{
	bool verbose;
} g_options;

void help()
{
	printf(
	"Usage: ft_ping [OPTION...] HOST ...\n"
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
	"Report bugs to <noalexan@student.42nice.fr>.\n");
	exit(EXIT_SUCCESS);
}

void usage()
{
	printf("Usage: ft_ping [-v?V] [--help] [--usage] [--version] HOST ...\n");
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

int main(int argc, char **argv)
{
	bzero(&g_options, sizeof(g_options));

	bool argument_parsing = true;
	for (int i = 1; i < argc; i++)
	{
		if (argument_parsing && argv[i][0] == '-' && argv[i][1])
		{
			if (argv[i][1] != '-')
			{
				for (int j = 1; argv[i][j]; j++)
				{
					switch (argv[i][j])
					{
					case 'v':
						g_options.verbose = true;
						break;

					case '?':
						help();
						break;

					case 'V':
						version();
						break;

					default:
						printf(
							"ft_ping: invalid option -- '%c'\n"
							"Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n",
						argv[i][j]);
						exit(EXIT_FAILURE);
					}
				}
			}

			else {
				if (strcmp(argv[i], "--") == 0) {
					argument_parsing = false;
				}

				else if (strcmp(argv[i], "--verbose") == 0) {
					g_options.verbose = true;
				}

				else if (strcmp(argv[i], "--help") == 0) {
					help();
				}

				else if (strcmp(argv[i], "--usage") == 0) {
					usage();
				}

				else if (strcmp(argv[i], "--version") == 0) {
					version();
				}

				else {
					printf(
						"ft_ping: unrecognized option '%s'\n"
						"Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n",
						argv[i]);
					exit(EXIT_FAILURE);
				}
			}
		}

		else
		{
			printf("host: %s\n", argv[i]);
		}
	}

	return 0;
}
