#include "http.h"

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/printk.h>

#define HTTP_PORT 8080

static int connect_socket(sa_family_t family, const char* server, int port, int* sock,
                          struct sockaddr* addr, socklen_t addr_len);

void http_init()
{
	struct sockaddr_in addr4;
	int                sock4 = -1;

	connect_socket(AF_INET,
	               CONFIG_NET_CONFIG_PEER_IPV4_ADDR,
	               HTTP_PORT,
	               &sock4,
	               (struct sockaddr*)&addr4,
	               sizeof(addr4));
}

static int connect_socket(sa_family_t family, const char* server, int port, int* sock,
                          struct sockaddr* addr, socklen_t addr_len)
{
	memset(addr, 0, addr_len);

	net_sin(addr)->sin_family = AF_INET;
	net_sin(addr)->sin_port   = htons(port);
	inet_pton(AF_INET, server, &net_sin(addr)->sin_addr);

	*sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*sock < 0) {
		printk("Failed to create %s HTTP socket (%d)", "IPv4", -errno);
		return -errno;
	}

	int ret = connect(*sock, addr, addr_len);
	if (ret < 0) {
		printk("Cannot connect to %s remote (%d)", "IPv4", -errno);
		close(*sock);
		*sock = -1;
		return -errno;
	}

	return 0;
}
