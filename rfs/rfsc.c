#define _XOPEN_SOURCE 600

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <io_file.h>

#include "rfsc.h"

struct state {
	int help_mode;
	char *host;
	char *port;
};

static struct state S = {
	.help_mode = 0,
	.host = NULL,
	.port = NULL,
};

static int sock = -1;

static bool rfs_connect(uint64_t last_key)
{
	const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *list, *p;
	if (getaddrinfo(S.host, S.port, &hints, &list) != 0)
		return false;

	for (p = list; p != NULL; p = p->ai_next) {
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sock == -1)
			continue;

		int keepalive = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
				&keepalive, sizeof(keepalive)) == -1)
			goto fail;

		int tcp_nodelay = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
				&tcp_nodelay, sizeof(tcp_nodelay)) == -1)
			goto fail;

		if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
			break;

	fail:
		close(sock);
		sock = -1;
	}

	freeaddrinfo(list);

	if (sock == -1)
		return false;

	ipc_init(&ipc);
	io_file_init(&ipc.io, sock);

	if (r_set_key(&ipc, &last_key) != 0) {
		close(sock);
		return false;
	}

	return true;
}

void rfs_destroy(void)
{
	if (sock != -1) {
		close(sock);
		sock = -1;
	}
}

bool rfs_recover(uint64_t last_key)
{
	rfs_destroy();
	return rfs_connect(last_key);
}

static struct fuse_opt fs_opts[] = {
	{"-h", offsetof(struct state, help_mode), 1},
	{"--help", offsetof(struct state, help_mode), 1},
	{"host=%s", offsetof(struct state, host), 0},
	{"port=%s", offsetof(struct state, port), 0},
	FUSE_OPT_END
};

static const char help_tmpl[] =
	"Usage: %s mountpoint [options]\n"
	"\n"
	"FS options:\n"
	"    -o host=HOST           server host\n"
	"    -o port=PORT           server port\n"
	"\n";

int main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (fuse_opt_parse(&args, &S, fs_opts, NULL) == -1)
		return 1;

	if (S.help_mode) {
		fprintf(stderr, help_tmpl, args.argv[0]);

		if (fuse_opt_add_arg(&args, "-ho") == -1)
			return 3;
	} else {
		if (fuse_opt_add_arg(&args, "-s") == -1)
			return 4;

		if (!rfs_connect(0))
			return 5;
	}

	return fuse_main(args.argc, args.argv, &fs_ops, NULL);
}
