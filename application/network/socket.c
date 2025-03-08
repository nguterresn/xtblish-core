#include "socket.h"
#include <zephyr/net/socket.h>

int socket_connect(int* sock, int type, int proto, struct sockaddr_in* addr,
                   int port)
{
	addr->sin_family = AF_INET;
	addr->sin_port   = htons(port);

	// *sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	*sock = socket(AF_INET, type, proto);
	if (*sock < 0) {
		printk("Failed to create IPv4 socket (%d)", -errno);
		return -errno;
	}

	int ret = zsock_connect(*sock,
	                        (struct sockaddr*)addr,
	                        sizeof(struct sockaddr_in));
	if (ret < 0) {
		printk("Cannot connect to IPv4 remote (%d)", -errno);
		zsock_close(*sock);
		*sock = -1;
		return -errno;
	}

	return 0;
}
