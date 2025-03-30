#include "socket.h"
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>

int socket_connect(int* sock, int type, int proto, const char* ip, int port)
{
	struct sockaddr_in sockaddr = { 0 };

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port   = htons(port);
	inet_pton(AF_INET, ip, &sockaddr.sin_addr);

	*sock = socket(AF_INET, type, proto);
	if (*sock < 0) {
		printk("Failed to create IPv4 socket (%d)", -errno);
		return -errno;
	}

	int ret = zsock_connect(*sock,
	                        (struct sockaddr*)&sockaddr,
	                        sizeof(struct sockaddr_in));
	if (ret < 0) {
		printk("Cannot connect to IPv4 remote (%d)", -errno);
		zsock_close(*sock);
		*sock = -1;
		return -errno;
	}

	return 0;
}
