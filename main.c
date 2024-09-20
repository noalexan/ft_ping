#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#include "ft_ping.h"

struct s_options g_options;
char *program_name;
bool stop = false;
int socket_fd;

enum
{
    ARG_TTL,
    ARG_USAGE,
};

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

void cleanup()
{
    if (close(socket_fd) < 0)
    {
        perror("ft_ping: close");
    }
}

static void sigint_handler()
{
    stop = true;
}

static size_t take_arg(size_t maxval, int allow_zero)
{
    char *endptr;
    size_t arg = strtoul(optarg, &endptr, 10);

    if (*endptr)
    {
        fprintf(stderr, "%s: invalid value (`%s' near `%s')\n",
                program_name, optarg, endptr);
        exit(EXIT_FAILURE);
    }

    if (arg == 0 && !allow_zero)
    {
        fprintf(stderr, "%s: option value too small: %s\n", program_name, optarg);
        exit(EXIT_FAILURE);
    }

    if (maxval && arg > maxval)
    {
        fprintf(stderr, "%s: option value too big: %s\n", program_name, optarg);
        exit(EXIT_FAILURE);
    }

    return arg;
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigint_handler);

    if ((program_name = *argv) == NULL)
    {
        fputs("A NULL argv[0] was passed through an exec system call.\n",
              stderr);
        abort();
    }

    g_options = (struct s_options){
        .count = -1,
        .ttl = 0,
        .verbose = false,
        .timeout = -1,
        .size = 56,
    };

    struct protoent *proto;

    proto = getprotobyname("icmp");
    if (proto == NULL)
    {
        fprintf(stderr, "ft_ping: unknown protocol icmp.\n");
        exit(EXIT_FAILURE);
    }

    socket_fd = socket(AF_INET, SOCK_RAW, proto->p_proto);
    if (socket_fd < 0)
    {
        if (errno == EPERM || errno == EACCES)
        {
            errno = 0;
            socket_fd = socket(AF_INET, SOCK_DGRAM, proto->p_proto);
            if (socket_fd < 0)
            {
                if (errno == EPERM || errno == EACCES || errno == EPROTONOSUPPORT)
                    fprintf(stderr, "ping: Lacking privilege for icmp socket.\n");
                else
                    fprintf(stderr, "ping: %s\n", strerror(errno));
            }
        }
    }

    atexit(cleanup);

    int socket_type = 0;
    while (true)
    {
        static struct option long_options[] = {
            {"count", required_argument, 0, 'c'},
            {"debug", no_argument, 0, 'd'},
            {"ttl", required_argument, 0, ARG_TTL},
            {"verbose", no_argument, 0, 'v'},
            {"timeout", required_argument, 0, 'w'},
            {"size", required_argument, 0, 's'},
            {"help", no_argument, 0, '?'},
            {"usage", no_argument, 0, ARG_USAGE},
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
            g_options.count = take_arg(0, true);
            break;

        case 'd':
            socket_type |= SO_DEBUG;
            break;

        case ARG_TTL:
            g_options.ttl = take_arg(255, false);
            break;

        case 'v':
            g_options.verbose = true;
            break;

        case 'w':
            g_options.timeout = take_arg(INT_MAX, 0);
            break;

        case 's':
            g_options.size = take_arg(PING_MAX_DATALEN, true);
            break;

        case ARG_USAGE:
            usage();
            break;

        case 'V':
            version();
            break;

        default:
            if (optopt == 0)
                help();

            fprintf(stderr, "Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr,
                "ft_ping: missing host operand\n"
                "Try 'ft_ping --help' or 'ft_ping --usage' for more information.\n");
        exit(EXIT_FAILURE);
    }

    int on = 1;
    if (socket_type != 0 &&
        setsockopt(socket_fd, SOL_SOCKET, socket_type, &on, sizeof on) < 0)
        perror("ft_ping: setsockopt");

    if (g_options.ttl > 0 &&
        setsockopt(socket_fd, IPPROTO_IP, IP_TTL, &g_options.ttl, sizeof g_options.ttl) < 0)
        perror("ft_ping: setsockopt");

    while (optind < argc)
        ft_ping(argv[optind++]);

    return 0;
}
