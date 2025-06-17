#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

#include <cmdsock.h>
#include <membuf.h>
#include <os.h>

#define BUFSIZE		1024

int server_fd, client_fd;

int cmdsock_start(const char *path)
{
	struct sockaddr_un addr;

	server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket");
		goto err;
	}

	memset(&addr, '\0', sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
	unlink(path);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		goto err_sock;
	}

	if (listen(server_fd, 1) == -1) {
		perror("listen");
		goto err_sock;
	}

	printf("cmdsock: listening on fd %d at %s\n", server_fd, path);

	return 0;

err_sock:
	close(server_fd);
err:
	return -1;
}

int cmdsock_poll(struct membuf *in, struct membuf *out)
{
	fd_set readfds, writefds;
	struct sockaddr_un addr;
	socklen_t len;
	char *ptr;
	int ret;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	if (!client_fd) {
		int fd;

		FD_SET(server_fd, &readfds);
		ret = select(server_fd + 1, &readfds, NULL, NULL, NULL);
		if (ret == -1) {
			perror("select");
			cmdsock_stop();
			return -1;
		}
		if (!ret)
			return 0;

		/* received an incoming connection */
		len = sizeof(addr);
		fd = accept(server_fd, (struct sockaddr *)&addr, &len);
		if (fd == -1) {
			perror("accept");
			return 0;
		}
		printf("cmdsock: connected\n");
		client_fd = fd;

		return 0;
	}

	FD_SET(client_fd, &readfds);
	FD_SET(client_fd, &writefds);
	ret = select(client_fd + 1, &readfds, &writefds, NULL, NULL);
	if (ret == -1) {
		perror("select");
		cmdsock_stop();
		return -1;
	}

	if (FD_ISSET(client_fd, &readfds)) {
		len = membuf_putraw(in, BUFSIZE, false, &ptr);
		if (len) {
			// os_printf("sb: can read %d\n", len);
			len = read(client_fd, ptr, len);
			// os_printf("sb: read %d\n", len);
			if (!len)
				goto disconnect;
			membuf_putraw(in, len, true, &ptr);
		}
	}

	if (FD_ISSET(client_fd, &writefds)) {
		len = membuf_getraw(out, BUFSIZE, false, &ptr);
		if (len) {
			// os_printf("sb: write %d\n", len);
			len = write(client_fd, ptr, len);
			if (!len)
				goto disconnect;
			membuf_getraw(out, len, true, &ptr);
		}
	}

	return 0;

disconnect:
	close(client_fd);
	client_fd = 0;
	printf("cmdsock: disconnected\n");

	return 0;
}

void cmdsock_stop(void)
{
	printf("cmdsock: closing\n");
	if (client_fd)
		close(client_fd);
	close(server_fd);
}

bool cmdsock_connected(void)
{
	return client_fd;
}
