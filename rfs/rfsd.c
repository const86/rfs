#define _XOPEN_SOURCE 600

#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <io_file.h>
#include "rfsd.h"

static struct ipc ipc;
static sig_atomic_t should_stop;

static int session(int sock)
{
	ipc_init(&ipc);
	io_file_init(&ipc.io, sock);

	rfs_init();
	while (ipc_process_rfs(&ipc) && !should_stop)
		;
	rfs_destroy();

	close(sock);
	return 0;
}

static void rfsd_shutdown(int sig)
{
	(void)sig;

	kill(0, SIGTERM);
	_exit(0);
}

static void rfsd_exit_one(int sig)
{
	(void)sig;

	should_stop = 1;

	const struct itimerval delay = {
		.it_interval = {1, 0},
	};
	if (setitimer(ITIMER_REAL, &delay, NULL) == -1)
		_exit(0);
}

static int setup_socket(const char *nodename, const char *servname)
{
	const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *list, *p;
	if (getaddrinfo(nodename, servname, &hints, &list) != 0)
		return -1;

	int sock = -1;
	for (p = list; p != NULL; p = p->ai_next) {
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sock == -1)
			continue;

		if (bind(sock, p->ai_addr, p->ai_addrlen) == 0 &&
			listen(sock, 1) == 0)
			break;

		close(sock);
		sock = -1;
	}

	freeaddrinfo(list);
	return sock;
}

static bool set_term_sigs(void (*handler)(int))
{
	struct sigaction sa = {.sa_handler = handler};

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);

	return sigaction(SIGINT, &sa, NULL) == 0 &&
		sigaction(SIGTERM, &sa, NULL) == 0;
}

int main(int argc, char **argv)
{
	if (argc != 3)
		return 1;

	if (setpgrp() == -1 || !set_term_sigs(rfsd_shutdown) ||
		sigignore(SIGCHLD) != 0)
		return 2;

	int sock = setup_socket(argv[1], argv[2]);
	if (sock == -1)
		return 3;

	sigset_t allsig;
	sigfillset(&allsig);

	for (;;) {
		int rsock = accept(sock, NULL, NULL);
		if (rsock == -1)
			continue;

		sigset_t oldmask;
		sigprocmask(SIG_BLOCK, &allsig, &oldmask);

		if (fork() == 0) {
			close(sock);

			if (!set_term_sigs(rfsd_exit_one))
				return 4;

			sigprocmask(SIG_SETMASK, &oldmask, NULL);
			return session(rsock);
		}

		close(rsock);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
	}
	close(sock);
	return 0;
}
