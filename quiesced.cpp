#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
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

int main(int argc, char *argv[]) {
	if (argc != 1) {
		fputs("Usage: quiesced\n", stderr);
		return 1;
	}

	// Wait just a moment to ensure that the elmlinkd has had time to
	// synchronize its channel index and provide us with the socket we expect.
	sleep(2);

	int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		perror("socket() failed");
		return 1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, "/var/run/elmlinkd/quiesce", sizeof(addr.sun_path) - 1);

	if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(sockaddr_un)) < 0) {
		close(sockfd);
		perror("connect() failed");
		return 1;
	}

	struct timespec next_announce;
	clock_gettime(CLOCK_MONOTONIC, &next_announce);
	next_announce.tv_sec += ANNOUNCE_INTERVAL;

	while (true) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		fd_set rfds, wfds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		FD_SET(sockfd, &rfds);

		int selectrv;
		if (timespec_le(next_announce, now)) {
			// We need you to return as soon as we can write.
			FD_SET(sockfd, &wfds);
			selectrv = pselect(sockfd + 1, &rfds, &wfds, NULL, NULL, NULL);
		}
		else {
			// We need you to return at our next announce interval so we can add to wfds.
			struct timespec timeout = timespec_sub(next_announce, now);
			selectrv = pselect(sockfd + 1, &rfds, &wfds, NULL, &timeout, NULL);
		}

		if (FD_ISSET(sockfd, &rfds)) {
			uint8_t buf[32]; // We don't expect large packets.
			int rv = recv(sockfd, buf, 32, 0);
			if (rv > 0 && std::string((const char *)buf, rv) == "QUIESCE_NOW") {
				printf("Quiesce requiested by IPMC.\n");
				fflush(stdout);
				close(sockfd);
				execlp("systemctl", "systemctl", "halt");
			}
		}
		if (FD_ISSET(sockfd, &wfds)) {
			// If we even put sockfd IN wfds, it's announce time.
			int rv = send(sockfd, "QUIESCE_SUBSCRIBE", strlen("QUIESCE_SUBSCRIBE"), 0);
			if (rv > 0) {
				clock_gettime(CLOCK_MONOTONIC, &next_announce);
				next_announce.tv_sec += ANNOUNCE_INTERVAL;
			}
		}
	}
	return 1; // This shouldn't happen.
}
