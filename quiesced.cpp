#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libledmgr.h>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// The number of seconds between QUIESCE_SUBSCRIBE announcements.
#define ANNOUNCE_INTERVAL 8

// The socket to use to communicate with the IPMC.
#define QUIESCE_SOCKET_PATH "/var/run/elmlinkd/quiesce"

static inline bool timespec_le(const struct timespec &a, const struct timespec &b) {
	if (a.tv_sec < b.tv_sec)
		return true;
	if (a.tv_sec > b.tv_sec)
		return false;
	return (a.tv_nsec <= b.tv_nsec);
}

static inline void timespec_normalize(struct timespec &c) {
	const int nanoscale = 1000000000;
	while (c.tv_nsec < 0) {
		--c.tv_sec;
		c.tv_nsec += nanoscale;
	}
	while (c.tv_nsec >= nanoscale) {
		++c.tv_sec;
		c.tv_nsec -= nanoscale;
	}
}

static inline struct timespec timespec_sub(const struct timespec &a, const struct timespec &b) {
	const int nanoscale = 1000000000;
	struct timespec c;
	c.tv_sec = a.tv_sec - b.tv_sec;
	c.tv_nsec = a.tv_nsec - b.tv_nsec;
	timespec_normalize(c);
	return c;
}

void indicate() {
	if (ledmgr_indicate_stubbed()) {
		printf("We did not detect the presence of libledmgr.so.1. We will not indicate.\n");
		return;
	}
	printf("Indicating...\n");
	time_t done = time(NULL) + 3;
	while (time(NULL) <= done) {
		ledmgr_indicate(0x000000, LEDMGR_DEFAULT_LED);
		usleep(80000);
	}
}

int main(int argc, char *argv[]) {
	if (argc != 1) {
		fputs("Usage: quiesced\n", stderr);
		return 1;
	}

	// Wait for the elmlinkd to synchronize its channel index and provide us
	// with the socket we expect.
	printf("Waiting for ELMLink quiesce socket.\n");
	while (access(QUIESCE_SOCKET_PATH, R_OK | W_OK) != 0)
		sleep(1);
	printf("Found ELMLink quiesce socket.\n");

	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		perror("socket() failed");
		return 1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, QUIESCE_SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(sockaddr_un)) < 0) {
		close(sockfd);
		perror("connect() failed");
		return 1;
	}

	printf("Connected. Waiting for quiesce signal.\n");

	while (true) {
		uint8_t buf[32]; // We don't expect large packets.
		int rv = recv(sockfd, buf, 32, 0);
		if (rv > 0 && std::string((const char *)buf, rv) == "QUIESCE_NOW") {
			printf("Quiesce requested by IPMC.\n");
			fflush(stdout);
			// Time to acknowledge it.
			send(sockfd, "QUIESCE_ACKNOWLEDGED", strlen("QUIESCE_ACKNOWLEDGED"), 0);
			close(sockfd);
			// We're going to spend 2-3 of our seconds just spamming the
			// white indicator, so front-panel users know we're about to
			// quiesce.
			indicate();
			printf("Quiescing!\n");
			execlp("systemctl", "systemctl", "halt");
		}
	}

	return 1; // This shouldn't happen.
}
