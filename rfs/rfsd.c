#define _XOPEN_SOURCE 600

#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <io_file.h>
#include "rfsd.h"

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

static struct ipc ipc;
static sig_atomic_t should_stop;
static sigjmp_buf exit_env;

static int session(int sock, const struct sockaddr *addr, socklen_t addr_len)
{
	char node[NI_MAXHOST];
	char service[NI_MAXSERV];
	int gnierr = getnameinfo(addr, addr_len, node, sizeof(node),
				service, sizeof(service),
				NI_NUMERICHOST | NI_NUMERICSERV);
	if (gnierr == 0)
		syslog(LOG_INFO, "New connection from [%s]:%s", node, service);
	else
		syslog(LOG_INFO, "New connection from unknown address "
			"(getnameinfo: %s)", gai_strerror(gnierr));

	ipc_init(&ipc);
	io_file_init(&ipc.io, sock);

	syslog(LOG_DEBUG, "Starting RFS session");
	rfs_init();

	while (ipc_process_rfs(&ipc) && !should_stop)
		;

	syslog(LOG_DEBUG, "Closing RFS session");
	rfs_destroy();

	if (close(sock) == -1)
		syslog(LOG_WARNING, "Closing client socket: %s",
			strerror(errno));

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
	syslog(LOG_INFO, "Requested to listen on %s:%s", nodename, servname);

	const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *list, *p;
	int gaierr = getaddrinfo(nodename, servname, &hints, &list);
	if (gaierr) {
		syslog(LOG_ERR, "Failed to resolve name: %s",
			gai_strerror(gaierr));
		return -1;
	}

	int sock = -1;
	for (p = list; p != NULL; p = p->ai_next) {
		char node[NI_MAXHOST];
		char service[NI_MAXSERV];
		int gnierr = getnameinfo(p->ai_addr, p->ai_addrlen, node,
					sizeof(node), service, sizeof(service),
					NI_NUMERICHOST | NI_NUMERICSERV);

		if (gnierr == 0)
			syslog(LOG_DEBUG, "Trying to listen on [%s]:%s",
				node, service);
		else
			syslog(LOG_DEBUG,
				"Trying to listen on unknown address "
				"(getnameinfo: %s", gai_strerror(gnierr));

		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sock == -1) {
			syslog(LOG_DEBUG, "Failed to open socket: %s",
				strerror(errno));
			continue;
		}

		if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
			syslog(LOG_DEBUG, "Failed to bind: %s",
				strerror(errno));
			goto fail;
		}

		if (listen(sock, 1) == -1) {
			syslog(LOG_DEBUG, "Failed to listen: %s",
				strerror(errno));
			goto fail;
		}

		int keepalive = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
				&keepalive, sizeof(keepalive)) == -1)
			syslog(LOG_NOTICE, "Failed to enable keep-alive: %s",
				strerror(errno));

		if (p->ai_protocol == IPPROTO_TCP) {
			int nodelay = 1;
			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
					&nodelay, sizeof(nodelay)) == -1)
				syslog(LOG_NOTICE,
					"Failed to enable TCP_NODELAY: %s",
					strerror(errno));
		}

		syslog(LOG_DEBUG, "Server socket is ready");
		break;

	fail:
		if (close(sock) == -1)
			syslog(LOG_DEBUG, "Closing socket: %s",
				strerror(errno));

		sock = -1;
	}

	freeaddrinfo(list);
	return sock;
}

static void main_loop(int sock)
{
	syslog(LOG_DEBUG, "Running main loop");

	sigset_t allsig;
	sigfillset(&allsig);

	for (;;) {
		struct sockaddr_storage addr;
		socklen_t addr_len = sizeof(addr);

		int rsock = accept(sock, (struct sockaddr *)&addr, &addr_len);
		if (rsock == -1) {
			syslog(LOG_WARNING, "Accepting new connection: %s",
				strerror(errno));
			continue;
		}

		sigset_t oldmask;
		sigprocmask(SIG_BLOCK, &allsig, &oldmask);

		if (fork() == 0) {
			set_term_sigs(rfsd_exit_one);
			sigprocmask(SIG_SETMASK, &oldmask, NULL);

			if (close(sock) == -1)
				syslog(LOG_WARNING,
					"Closing listen socket: %s",
					strerror(errno));

			session(rsock, (struct sockaddr *)&addr, addr_len);
			return;
		}

		sigprocmask(SIG_SETMASK, &oldmask, NULL);

		if (close(rsock) == -1)
			syslog(LOG_WARNING, "Closing client socket: %s",
				strerror(errno));
	}
}

int main(int argc, char **argv)
{
	openlog(argv[0],
#ifdef LOG_PERROR
		LOG_PERROR |
#endif
		LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);

	if (argc != 3) {
		syslog(LOG_EMERG,
			"%d arguments passed, while 2 are required", argc);
		return 1;
	}

	setpgrp();
	set_term_sigs(rfsd_shutdown);
	sigignore(SIGCHLD);

	int sock = setup_socket(argv[1], argv[2]);
	if (sock == -1) {
		syslog(LOG_EMERG, "Cannot listen requested address!");
		return 1;
	}

	if (!sigsetjmp(exit_env, 0))
		main_loop(sock);
	else {
		syslog(LOG_DEBUG, "Stopping the server");

		if (close(sock) == -1)
			syslog(LOG_WARNING, "Closing listen socket: %s",
				strerror(errno));

		syslog(LOG_DEBUG, "Waiting for children...");
		while (wait(NULL) != -1 || errno != ECHILD)
			;
	}

	syslog(LOG_INFO, "Bye-bye!");
	closelog();
	return 0;
}
