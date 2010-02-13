#define _XOPEN_SOURCE 600

#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include <io_file.h>
#include "rfsd.h"

static struct ipc ipc;
static sig_atomic_t should_stop;
static sigjmp_buf exit_env;

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

static void set_term_sigs(void (*handler)(int))
{
	struct sigaction sa = {.sa_handler = handler};

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

static void rfsd_shutdown(int sig)
{
	kill(0, sig);
	siglongjmp(exit_env, 1);
}

static void rfsd_exit_one(int sig)
{
	(void)sig;

	set_term_sigs(SIG_IGN);
	should_stop = 1;

	const struct itimerval delay = {
		.it_value = {1, 0},
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

		int keepalive = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
				&keepalive, sizeof(keepalive)) == -1)
			goto fail;

		int tcp_nodelay = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
				&tcp_nodelay, sizeof(tcp_nodelay)) == -1)
			goto fail;

		if (bind(sock, p->ai_addr, p->ai_addrlen) == 0 &&
			listen(sock, 1) == 0)
			break;

	fail:
		close(sock);
		sock = -1;
	}

	freeaddrinfo(list);
	return sock;
}

static int main_loop(int sock)
{
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

			set_term_sigs(rfsd_exit_one);
			sigprocmask(SIG_SETMASK, &oldmask, NULL);

			return session(rsock);
		}

		close(rsock);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 3)
		return 1;

	setpgrp();
	set_term_sigs(rfsd_shutdown);
	sigignore(SIGCHLD);

	int sock = setup_socket(argv[1], argv[2]);
	if (sock == -1)
		return 3;

	if (!sigsetjmp(exit_env, 0))
		return main_loop(sock);

	close(sock);

	while (wait(NULL) != -1 || errno != ECHILD)
		;

	return 0;
}
